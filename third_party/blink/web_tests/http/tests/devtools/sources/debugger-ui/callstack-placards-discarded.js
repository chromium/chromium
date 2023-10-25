// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as BindingsModule from 'devtools/models/bindings/bindings.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that RawSourceCode listeners count won't grow on each script pause. Bug 70996\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
      }
  `);

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.runDebuggerTestSuite([
    function testCallStackPlacardsDiscarded(next) {
      TestRunner.debuggerModel.addEventListener(SDK.DebuggerModel.Events.DebuggerPaused, didPause, this);
      var previousCount = undefined;
      function didPause(event) {
        TestRunner.addResult('Received DebuggerPaused event.');
        var callFrame = event.data.callFrames[0];
        TestRunner.addResult('Function name: ' + callFrame.functionName);
        var count = liveLocationsCount();
        if (previousCount !== undefined && count !== previousCount)
          TestRunner.addResult('FAILED: Live locations count has changed!');
        previousCount = count;
      }

      SourcesTestRunner.showScriptSource('callstack-placards-discarded.js', didShowScriptSource);
      function didShowScriptSource(sourceFrame) {
        TestRunner.addResult('Script source was shown.');
        SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause1);
      }
      function didPause1() {
        TestRunner.addResult('Script execution resumed.');
        SourcesTestRunner.resumeExecution(didResume1);
      }
      function didResume1() {
        SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause2);
      }
      function didPause2() {
        TestRunner.addResult('Script execution resumed.');
        SourcesTestRunner.resumeExecution(didResume2);
      }
      function didResume2() {
        next();
      }
    },
  ]);

  function liveLocationsCount() {
    var count = 0;
    var infos = Object.values(TestRunner.debuggerModel.scripts)
                    .map(script => script[BindingsModule.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.ScriptInfoSymbol])
                    .filter(info => !!info);
    infos.forEach(function(info) {
      count += info.locations ? info._locations.size : 0;
    });
    return count;
  }
})();
