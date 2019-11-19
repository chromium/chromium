// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the original content is accessible on live edited scripts.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/edit-me.js');

  SourcesTestRunner.startDebuggerTest(testStarted);
  function testStarted() {
    SourcesTestRunner.showScriptSource('edit-me.js', didShowScriptSource);
  }

  function didShowScriptSource(sourceFrame) {
    replaceInSource(sourceFrame, 'return 0;', 'return "live-edited string";', didEditScriptSource);
  }

  function didEditScriptSource() {
    WorkspaceDiff.workspaceDiff().requestOriginalContentForUISourceCode(UI.panels.sources.sourcesView().currentUISourceCode()).then(gotOriginalContent);
  }

  function gotOriginalContent(originalContent) {
    TestRunner.addResult('==== Original Content ====');
    TestRunner.addResult(originalContent);
    UI.panels.sources.sourcesView().currentUISourceCode().requestContent().then(gotContent);
  }

  function gotContent({ content, error, isEncoded }) {
    TestRunner.addResult('');
    TestRunner.addResult('');
    TestRunner.addResult('==== Current Content ====');
    TestRunner.addResult(content);
    SourcesTestRunner.completeDebuggerTest();
  }

  function replaceInSource(sourceFrame, string, replacement, callback) {
    TestRunner.addSniffer(TestRunner.debuggerModel, '_didEditScriptSource', callback);
    SourcesTestRunner.replaceInSource(sourceFrame, string, replacement);
    SourcesTestRunner.commitSource(sourceFrame);
  }
})();
