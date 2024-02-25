// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Root from 'devtools/core/root/root.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Sources from 'devtools/panels/sources/sources.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests selected call frame does not change when pretty-print is toggled.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          return testFunction2();
      }

      function testFunction2()
      {
          var x = Math.sqrt(10);
          debugger;
          return x;
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);
  var panel = Sources.SourcesPanel.SourcesPanel.instance();
  var sourceFrame;

  function step1() {
    var testName = Root.Runtime.Runtime.queryParam('test');
    testName = testName.substring(testName.lastIndexOf('/') + 1);
    SourcesTestRunner.showScriptSource(testName, step2);
  }

  function step2(frame) {
    sourceFrame = frame;
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step3);
  }

  function step3() {
    SourcesTestRunner.completeDebuggerTest();
    return;
    TestRunner.debuggerModel.setSelectedCallFrame(TestRunner.debuggerModel.debuggerPausedDetails().callFrames[1]);
    sourceFrame.toggleFormatSource(step4);
  }

  function step4() {
    TestRunner.assertEquals('testFunction', UI.Context.Context.instance().flavor(SDK.DebuggerModel.CallFrame).functionName);
    sourceFrame.toggleFormatSource(step5);
  }

  function step5() {
    SourcesTestRunner.completeDebuggerTest();
  }
})();
