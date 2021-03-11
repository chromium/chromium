// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that Linkifier works correctly.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadLegacyModule('components');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`function foo () {} //# sourceURL=linkifier.js`)
  var script;
  var sourceURL = 'linkifier.js';
  SourcesTestRunner.startDebuggerTest(waitForScripts);

  function waitForScripts() {
    SourcesTestRunner.showScriptSource(sourceURL, debuggerTest);
  }

  function debuggerTest() {
    var target = SDK.targetManager.mainTarget();
    for (var scriptCandidate of TestRunner.debuggerModel.scripts()) {
      if (scriptCandidate.sourceURL === sourceURL) {
        script = scriptCandidate;
        break;
      }
    }

    dumpLiveLocationsCount();

    var linkifier = new Components.Linkifier();
    TestRunner.addResult('Created linkifier');
    dumpLiveLocationsCount();

    var linkA = linkifier.linkifyScriptLocation(target, null, sourceURL, 10);
    TestRunner.addResult('Linkified script location A');
    dumpLiveLocationsCount();

    var linkB = linkifier.linkifyScriptLocation(target, null, sourceURL, 15);
    TestRunner.addResult('Linkified script location B');
    dumpLiveLocationsCount();

    linkifier.reset();
    TestRunner.addResult('Reseted linkifier');
    dumpLiveLocationsCount();

    linkifier.dispose();
    TestRunner.addResult('Disposed linkifier');
    dumpLiveLocationsCount();

    TestRunner.completeTest();
  }

  function dumpLiveLocationsCount() {
    var modelData = Bindings.debuggerWorkspaceBinding._debuggerModelToData.get(script.debuggerModel);
    var locations = modelData._locations.get(script.scriptId);
    TestRunner.addResult('Live locations count: ' + locations.size);
    TestRunner.addResult('');
  }
})();
