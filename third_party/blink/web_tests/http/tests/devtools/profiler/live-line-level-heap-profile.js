// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the live line-level heap profile is shown in the text editor.\n`);
  Common.settingForTest('memoryLiveHeapProfile').set(true);
  await self.runtime.loadModulePromise('perf_ui');
  await Main.Main._instanceForTest.lateInitDonePromiseForTest();
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.evaluateInPagePromise(`
      let dump = new Array(10000).fill(42).map(x => Date.now() + '42');
      //# sourceURL=allocator.js`);

  TestRunner.addSniffer(SourceFrame.SourcesTextEditor.prototype, 'setGutterDecoration', decorationAdded, true);
  SourcesTestRunner.showScriptSource('allocator.js', frameRevealed);

  function decorationAdded(line, type, element) {
    if (line !== 13 || type !== 'CodeMirror-gutter-memory' || !element.textContent || !element.style.backgroundColor)
      return;
    TestRunner.addResult(`Memory annotation added to line ${line}.`);
    TestRunner.completeTest();
  }

  function frameRevealed(frame) {
    TestRunner.addResult(TestRunner.formatters.formatAsURL(frame.uiSourceCode().url()));
  }
})();
