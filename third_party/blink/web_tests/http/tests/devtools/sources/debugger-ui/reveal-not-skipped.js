// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  Root.Runtime.experiments.setEnabled('sourcesPrettyPrint', false);

  TestRunner.addResult(
      `Tests that certain user actions in scripts panel reveal execution line.\n`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('resources/reveal-not-skipped.html');

  var panel = UI.panels.sources;
  TestRunner.DebuggerAgent.setPauseOnExceptions(
      SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);

  SourcesTestRunner.runDebuggerTestSuite([
    function testRevealAfterPausedOnException(next) {
      TestRunner.addResult('Showing script1 source...');
      SourcesTestRunner.showScriptSource('script1.js', step2);

      function step2() {
        TestRunner.addResult(
            'Script source was shown for \'' +
            panel.visibleView.uiSourceCode().name() + '\'.');
        TestRunner.addResult('Throwing exception...');
        TestRunner.evaluateInPage('setTimeout(throwAnException, 0)');
        TestRunner.addSniffer(
            Sources.TabbedEditorContainer.prototype, 'showFile', step3);
      }

      function step3() {
        TestRunner.addResult(
            'Script source was shown for \'' +
            panel.visibleView.uiSourceCode().name() + '\'.');
        TestRunner.addResult('Reloading page...');
        TestRunner.reloadPage(step4);
      }

      function step4() {
        TestRunner.addResult('Showing script1 source...');
        SourcesTestRunner.showScriptSource('script1.js', step5);
      }

      function step5() {
        TestRunner.addResult(
            'Script source was shown for \'' +
            panel.visibleView.uiSourceCode().name() + '\'.');
        TestRunner.addResult('Throwing exception...');
        TestRunner.evaluateInPage('setTimeout(throwAnException, 0)');
        TestRunner.addSniffer(
            Sources.TabbedEditorContainer.prototype, 'showFile', step6);
      }

      function step6() {
        TestRunner.addResult(
            'Script source was shown for \'' +
            panel.visibleView.uiSourceCode().name() + '\'.');
        next();
      }
    },

    function testRevealAfterPrettyPrintWhenPaused(next) {
      TestRunner.addResult('Throwing exception...');
      SourcesTestRunner.waitUntilPaused(step2);
      function step2() {
        TestRunner.addResult('Showing script1 source...');
        SourcesTestRunner.showScriptSource('script1.js', step3);
      }

      function step3() {
        TestRunner.addResult(
            'Script source was shown for \'' +
            panel.visibleView.uiSourceCode().name() + '\'.');
        TestRunner.addResult('Formatting...');
        SourcesTestRunner.scriptFormatter().then(function(scriptFormatter) {
          TestRunner.addSniffer(
              Sources.ScriptFormatterEditorAction.prototype, 'updateButton',
              uiSourceCodeScriptFormatted);
          scriptFormatter.toggleFormatScriptSource();
        });
      }

      function uiSourceCodeScriptFormatted() {
        TestRunner.addResult(
            'Script source was shown for \'' +
            panel.visibleView.uiSourceCode().name() + '\'.');
        next();
      }
    }
  ]);
})();
