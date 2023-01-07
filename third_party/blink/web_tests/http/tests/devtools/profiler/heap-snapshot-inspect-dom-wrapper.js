// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that resolving heap snapshot object to a JS object will not crash on DOM wrapper boilerplate. Test passes if it doesn't crash.\n`);
  await TestRunner.loadTestModule('heap_profiler_test_runner');
  await TestRunner.showPanel('heap_profiler');
  await TestRunner.evaluateInPagePromise(`
      // Make sure there is a body wrapper.
      document.body.fieldOnDomWrapper = 2012;
  `);

  var heapProfileType = Profiler.ProfileTypeRegistry.instance.heapSnapshotProfileType;
  heapProfileType.addEventListener(Profiler.HeapSnapshotProfileType.SnapshotReceived, finishHeapSnapshot);
  TestRunner.addSniffer(heapProfileType, 'snapshotReceived', snapshotReceived);
  heapProfileType.takeHeapSnapshot();

  function finishHeapSnapshot(uid) {
    TestRunner.addResult('PASS: snapshot was taken');
    var profiles = heapProfileType.getProfiles();

    if (!profiles.length)
      return clear('FAILED: no profiles found');

    if (profiles.length > 1)
      return clear('FAILED: wrong number of recorded profiles was found. profiles.length = ' + profiles.length);

    var profile = profiles[profiles.length - 1];
    UI.panels.heap_profiler.showProfile(profile);
  }

  async function snapshotReceived(profile) {
    TestRunner.addResult('PASS: snapshot was received');
    var snapshotProxy = profile.snapshotProxy;
    var bodyWrapperIds = await snapshotProxy.callMethodPromise('idsOfObjectsWithName', 'HTMLBodyElement');

    if (bodyWrapperIds.length < 3)
      return clear('FAILED: less than 3 HTMLBodyElement objects were detected');

    TestRunner.addResult('PASS: more than 2 HTMLBodyElements were found');

    var remoteObjects = [];
    for (var i = 0; i < bodyWrapperIds.length; i++) {
      var object = await TestRunner.HeapProfilerAgent.getObjectByHeapObjectId('' + bodyWrapperIds[i]);
      if (object)
        remoteObjects.push(TestRunner.runtimeModel.createRemoteObject(object));
    }

    if (!remoteObjects.length)
      return clear('FAILED: no resolved objects were detected');

    TestRunner.addResult('PASS: got at least one HTMLBodyElement wrapper');

    for (var remoteObject of remoteObjects)
      await remoteObject.getOwnProperties(false);

    clear();
  }

  function clear(errorMessage) {
    if (errorMessage)
      TestRunner.addResult(errorMessage);
    setTimeout(done, 0);
    UI.panels.heap_profiler.reset();
    return !errorMessage;
  }

  function done() {
    TestRunner.addResult('PASS: profile was deleted');
    TestRunner.completeTest();
  }
})();
