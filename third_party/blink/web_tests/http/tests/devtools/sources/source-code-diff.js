// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that diff markers correctly appear in the gutter.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('resources/diff-before.css');
  await TestRunner.addStylesheetTag('resources/diff-after.css');

  Runtime.experiments.enableForTest('sourceDiff');
  var textAfter;
  SourcesTestRunner.waitForScriptSource(
      'diff-after.css', uiSourceCode => uiSourceCode.requestContent().then(onAfterContent));

  function onAfterContent({ content, error, isEncoded }) {
    textAfter = content;
    SourcesTestRunner.waitForScriptSource('diff-before.css', onBeforeUISourceCode);
  }

  function onBeforeUISourceCode(uiSourceCode) {
    uiSourceCode.setWorkingCopy(textAfter);
    TestRunner.addSniffer(
        Sources.GutterDiffPlugin.prototype, '_decorationsSetForTest',
        decorationsSet);
    SourcesTestRunner.showUISourceCodePromise(uiSourceCode);
  }

  function decorationsSet(decorations) {
    Array.from(decorations).sort((a, b) => a[0] - b[0]).forEach(print);
    TestRunner.completeTest();

    function print(decoration) {
      var type = decoration[1].type;
      var name = 'Unknown';
      if (type === SourceFrame.SourceCodeDiff.EditType.Insert)
        name = 'Insert';
      else if (type === SourceFrame.SourceCodeDiff.EditType.Delete)
        name = 'Delete';
      else if (type === SourceFrame.SourceCodeDiff.EditType.Modify)
        name = 'Modify';

      TestRunner.addResult(decoration[0] + ':' + name);
    }
  }
})();
