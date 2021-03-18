// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test verifies that the text editor can be read by assistive technology.\n`);
  await TestRunner.loadModule('text_editor');
  await TestRunner.loadLegacyModule('text_editor');
  const editorFactory = TextEditor.CodeMirrorTextEditorFactory.instance()

  let editor = editorFactory.createEditor({lineWrapping: false});
  editor.widget().show(UI.inspectorView.element);
  TestRunner.addResult('Without line wrapping:')
  editor.focus();
  editor.setText(`this is the text on the first line
this is the text on the second line
third line
fourth line`);
  dumpScreenreader();

  press('ArrowDown');
  press('ArrowDown');
  press('ArrowDown');
  press('ArrowDown');
  press('ArrowUp');
  press('ArrowUp');
  press('ArrowUp');

  TestRunner.addResult('Selecting All');
  editor.setSelection(editor.fullRange());
  dumpScreenreader();


  TestRunner.addResult('\nWith line wrapping:')
  editor = editorFactory.createEditor({lineWrapping: true});
  editor.widget().element.style = 'overflow:hidden; width: 100px;';
  editor.widget().show(UI.inspectorView.element);
  editor.setText('a'.repeat(100) + ' ' + 'b'.repeat(100) + ' ' + 'c'.repeat(100));
  editor.focus();
  dumpScreenreader();
  press('ArrowDown');
  press('ArrowDown');
  press('ArrowDown');
  press('ArrowUp');
  press('ArrowUp');
  press('ArrowUp');

  TestRunner.completeTest();

  function press(key) {
    TestRunner.addResult('Pressing ' + key);
    eventSender.keyDown(key);
    dumpScreenreader();
  }

  function dumpScreenreader() {
    TestRunner.addResult(indent(document.deepActiveElement().value || document.deepActiveElement().textContent));

    function indent(text) {
      return text.split('\n').map(line => '  ' + line).join('\n')
    }
  }
})();
