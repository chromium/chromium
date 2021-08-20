// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies that editing a pretty printed resource works properly.\n`);

  Root.Runtime.experiments.enableForTest('sourcesPrettyPrint');
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.addScriptTag('resources/ugly-function.js');

  const sourceFrame = await SourcesTestRunner.showScriptSourcePromise('ugly-function.js');
  dumpState('Initial state');

  await sourceFrame.setPretty(true);
  dumpState('Toggle pretty print on');

  await new Promise(x => SourcesTestRunner.typeIn(sourceFrame.textEditor, 'X', x));
  dumpState('Type in "X"');

  sourceFrame.textEditor.codeMirror().execCommand('undo');
  dumpState('Undo');

  await sourceFrame.setPretty(false);
  dumpState('Toggle pretty print off');

  TestRunner.completeTest();

  function dumpState(info) {
    const uiSourceCode = sourceFrame.uiSourceCode();
    const button = sourceFrame.prettyToggle;
    let buttonState = 'invisible';
    button.element.disabled
    if (button.visible()) {
      buttonState = button.toggled() ? 'on' : 'off';
      if (button.element.disabled)
        buttonState += ' disabled';
    }

    TestRunner.addResult(`* ${info} *`);
    TestRunner.addResult(`pretty print button: ${buttonState}`);
    TestRunner.addResult(`text: ${sourceFrame.textEditor.text().trim()}`);
    TestRunner.addResult(`isDirty: ${uiSourceCode.isDirty()}`);
    TestRunner.addResult(`workingCopy: ${uiSourceCode.workingCopy().trim()}`);
    TestRunner.addResult('');
  }
})();
