// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test verifies text editor's enabling and disabling tab moves focus behavior\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  var textEditor = SourcesTestRunner.createTestEditor();
  textEditor.setMimeType('text/javascript');
  textEditor.setReadOnly(false);
  const text = `function foo() {
  var x = 11;
  return x
}`;

  function dumpFocus() {
    var element = document.deepActiveElement();
    if (!element) {
      TestRunner.addResult('null');
      return;
    }
    var name = element.tagName;
    if (element.id)
      name += '#' + element.id;
    if (element.getAttribute('aria-label'))
      name += ':' + element.getAttribute('aria-label');
    else if (element.title)
      name += ':' + element.title;
    else if (element.textContent && element.textContent.length < 50) {
      name += ':' + element.textContent.replace('\u200B', '');
    } else if (element.className)
      name += '.' + element.className.split(' ').join('.');
    TestRunner.addResult(name);
  }

  function setUpTextEditor(text) {
    textEditor.setText(text);
    textEditor.focus();
    SourcesTestRunner.setLineSelections(textEditor, [{line: 0, column: 0}]);
  }

  function pressTabAndDump(next) {
    TestRunner.addResult(`Focus is on:`);
    dumpFocus();
    eventSender.keyDown('Tab');
    TestRunner.addResult(`After pressing tab, focus is on:`);
    dumpFocus();
    SourcesTestRunner.dumpTextWithSelection(textEditor, true);
  }

  var testSuite = [
    function testDefaultTabBehavior(next) {
      setUpTextEditor(text);

      pressTabAndDump();
      next();
    },

    function testEnableTabMovesFocus(next) {
      setUpTextEditor(text);

      Common.moduleSetting('textEditorTabMovesFocus').set(true);
      TestRunner.addResult(`Enable tab moves focus and press Tab key`);
      pressTabAndDump();
      eventSender.keyDown('Tab', ['shiftKey']);
      TestRunner.addResult(`After pressing shift + tab, focus is on:`);
      dumpFocus();
      next();
    },

    function testDisableTabMovesFocus(next) {
      setUpTextEditor(text);

      Common.moduleSetting('textEditorTabMovesFocus').set(false);
      TestRunner.addResult(`Disable tab moves focus and press Tab key`);
      pressTabAndDump();
      next();
    },
  ];
  TestRunner.runTestSuite(testSuite);
})();