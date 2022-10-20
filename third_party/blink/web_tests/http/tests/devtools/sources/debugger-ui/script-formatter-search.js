// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  Root.Runtime.experiments.setEnabled('sourcesPrettyPrint', false);

  TestRunner.addResult(
      `Tests that search across files works with formatted scripts.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function nonFormattedFunction() { var  i = 2 + 2; var a = 4; return a + i; }
      console.log('magic-string'); // first
      console.log('magic-string'); // second
      //# sourceURL=test.js
  `);

  var scriptSource;
  var shouldRequestContent = false;
  SourcesTestRunner.scriptFormatter().then(startDebuggerTest);
  var scriptFormatter;

  function startDebuggerTest(sf) {
    scriptFormatter = sf;
    SourcesTestRunner.startDebuggerTest(started);
  }

  function started() {
    SourcesTestRunner.showScriptSource('test.js', didShowScriptSource);
  }

  async function didShowScriptSource(frame) {
    scriptSource = frame.uiSourceCode();
    var matches =
        await scriptSource.searchInContent('magic-string', true, false);
    TestRunner.addResult('Pre-format search results:');
    SourcesTestRunner.dumpSearchMatches(matches);
    shouldRequestContent = true;
    TestRunner.addSniffer(
        Sources.ScriptFormatterEditorAction.prototype, 'updateButton',
        uiSourceCodeScriptFormatted);
    scriptFormatter.toggleFormatScriptSource();
  }

  async function uiSourceCodeScriptFormatted() {
    var matches =
        await scriptSource.searchInContent('magic-string', true, false);
    TestRunner.addResult('Post-format search results:');
    SourcesTestRunner.dumpSearchMatches(matches);
    TestRunner.completeTest();
  }
})();
