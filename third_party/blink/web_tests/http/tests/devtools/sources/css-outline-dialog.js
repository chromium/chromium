// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Top-down test to verify css outline dialog.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('resources/css-outline-column.css');

  SourcesTestRunner.showScriptSource('css-outline-column.css', onSourceShown);
  var textEditor;
  function onSourceShown(sourceFrame) {
    textEditor = sourceFrame.textEditor;
    TestRunner.addSniffer(Sources.OutlineQuickOpen.prototype, 'refresh', onQuickOpenFulfilled);
    UI.panels.sources._sourcesView._showOutlineQuickOpen();
  }

  function onQuickOpenFulfilled() {
    TestRunner.addSniffer(Common.Revealer, 'reveal', (revealable, promise) => promise.then(revealed));
    this.selectItem(1, '');
  }

  function revealed() {
    var selection = textEditor.selection();
    if (!selection.isEmpty()) {
      TestRunner.addResult('ERROR: selection is not empty.');
      TestRunner.completeTest();
      return;
    }
    TestRunner.addResult(
        String.sprintf('Cursor position: line %d, column %d', selection.startLine, selection.startColumn));
    TestRunner.completeTest();
  }
})();
