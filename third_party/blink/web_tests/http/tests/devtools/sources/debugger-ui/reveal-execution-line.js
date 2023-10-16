// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SourceFrame from 'devtools/ui/legacy/components/source_frame/source_frame.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';

(async function() {
  TestRunner.addResult(`Tests that execution line is revealed and highlighted when debugger is paused.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          debugger;
      }
  `);

  SourcesTestRunner.setQuiet(true);
  SourcesTestRunner.runDebuggerTestSuite([function testRevealAndHighlightExecutionLine(next) {
    var executionLineSet = false;
    var executionLineRevealed = false;
    TestRunner.addSniffer(SourceFrame.SourceFrame.SourceFrameImpl.prototype, 'revealPosition', didRevealLine);
    TestRunner.addSniffer(
        SourcesModule.DebuggerPlugin.DebuggerPlugin.prototype, '_executionLineChanged',
        didSetExecutionLocation);
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(didPause);

    function didPause(callFrames) {
    }

    function didSetExecutionLocation(uiLocation) {
      if (executionLineSet)
        return;
      executionLineSet = true;
      maybeNext();
    }

    function didRevealLine(line) {
      if (executionLineRevealed)
        return;
      if (this.isShowing()) {
        executionLineRevealed = true;
        maybeNext();
      }
    }

    function maybeNext() {
      if (executionLineRevealed && executionLineSet) {
        TestRunner.addResult('Execution line revealed and highlighted.');
        SourcesTestRunner.resumeExecution(next);
      }
    }
  }]);
})();
