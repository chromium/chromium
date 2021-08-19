// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies proactive javascript compilation.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('debugger/resources/edit-me.js');

  SourcesTestRunner.showScriptSource('edit-me.js', onSourceFrame);

  function onSourceFrame(sourceFrame) {
    TestRunner.addSniffer(
        Sources.JavaScriptCompilerPlugin.prototype, 'compilationFinishedForTest',
        onCompilationFinished.bind(null, sourceFrame));
    sourceFrame.textEditor.setSelection(TextUtils.TextRange.createFromLocation(0, 0));
    SourcesTestRunner.typeIn(sourceFrame.textEditor, 'test!');
  }

  function onCompilationFinished(sourceFrame) {
    SourcesTestRunner.dumpSourceFrameMessages(sourceFrame);
    TestRunner.completeTest();
  }
})();
