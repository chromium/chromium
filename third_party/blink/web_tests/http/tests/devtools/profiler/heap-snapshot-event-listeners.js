// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that event listeners not user reachable from the root are still present in the class list.\n`);
  await TestRunner.loadTestModule('heap_profiler_test_runner');
  await TestRunner.showPanel('heap_profiler');
  await TestRunner.evaluateInPagePromise(`
      class EventListenerWrapperTest {
        constructor() {
          this.x = 42;
          window.addEventListener('mousemove', event => console.log(this.x));
        }
      }

      new EventListenerWrapperTest();
  `);

  var heapProfileType = Profiler.ProfileTypeRegistry.instance.heapSnapshotProfileType;
  heapProfileType.addEventListener(Profiler.HeapSnapshotProfileType.SnapshotReceived, finishHeapSnapshot);
  TestRunner.addSniffer(heapProfileType, 'snapshotReceived', snapshotReceived);
  heapProfileType.takeHeapSnapshot();

  function finishHeapSnapshot(uid) {
    var profiles = heapProfileType.getProfiles();

    if (!profiles.length)
      return clear('FAILED: no profiles found');

    if (profiles.length > 1)
      return clear('FAILED: wrong number of recorded profiles was found. profiles.length = ' + profiles.length);

    var profile = profiles[profiles.length - 1];
    UI.panels.heap_profiler.showProfile(profile);
  }

  async function snapshotReceived(profile) {
    var snapshotProxy = profile.snapshotProxy;
    var classNames = await snapshotProxy.aggregatesWithFilter(new HeapSnapshotModel.NodeFilter());
    var found = Object.keys(classNames).includes('EventListenerWrapperTest');
    if (found)
      TestRunner.addResult('PASS: the class name is found');
    else
      TestRunner.addResult('ERROR: the class name is not found.');

    TestRunner.completeTest();
  }

  function clear(errorMessage) {
    if (errorMessage)
      TestRunner.addResult(errorMessage);
    TestRunner.completeTest();
  }
})();
