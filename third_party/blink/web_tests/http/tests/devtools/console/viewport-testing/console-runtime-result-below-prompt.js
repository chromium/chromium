// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console fills the empty element below the prompt editor.\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await ConsoleTestRunner.waitForPendingViewportUpdates();
  const consoleView = Console.ConsoleView.instance();
  const prompt = consoleView._prompt;
  const editor = await ConsoleTestRunner.waitUntilConsoleEditorLoaded();
  Common.settings.moduleSetting('consoleEagerEval').set(true);

  TestRunner.runTestSuite([
    async function testUnsafeExpressions(next) {
      await checkExpression(`var should_not_be_defined`);
      await checkExpression(`window.prop_should_not_be_set = 1`);

      await evaluateAndDumpResult(`should_not_be_defined`);
      await evaluateAndDumpResult(`window.prop_should_not_be_set`);

      next();
    },

    async function testSafeExpressions(next) {
      await checkExpression(`1 + 2`);
      await checkExpression(`123`);

      next();
    },

    async function testNoOpForLongText(next) {
      TestRunner.addResult('Setting max length for evaluation to 0');
      const originalMaxLength = ObjectUI.JavaScriptREPL._MaxLengthForEvaluation;
      ObjectUI.JavaScriptREPL._MaxLengthForEvaluation = 0;
      await checkExpression(`1 + 2`);
      ObjectUI.JavaScriptREPL._MaxLengthForEvaluation = originalMaxLength;

      next();
    },

    async function testClickingPreviewFocusesEditor(next) {
      const expression = `1 + 2`;
      TestRunner.addResult(`Prompt text set to \`${expression}\``);
      prompt.setText(expression);
      await prompt._previewRequestForTest;

      editor.setSelection(TextUtils.TextRange.createFromLocation(0, 0));
      TestRunner.addResult('Selection before: ' + editor.selection().toString());

      TestRunner.addResult(`Clicking preview element`);
      prompt._eagerPreviewElement.click();

      TestRunner.addResult('Selection after: ' + editor.selection().toString());
      TestRunner.addResult(`Editor has focus: ${editor.element.hasFocus()}`);

      next();
    },

    async function testClickWithSelectionDoesNotFocusEditor(next) {
      const expression = `1 + 2`;
      TestRunner.addResult(`Prompt text set to \`${expression}\``);
      prompt.setText(expression);
      await prompt._previewRequestForTest;
      document.activeElement.blur();

      var firstTextNode = prompt.belowEditorElement().traverseNextTextNode();
      window.getSelection().setBaseAndExtent(firstTextNode, 0, firstTextNode, 1);
      TestRunner.addResult('Selection before: ' + window.getSelection().toString());

      TestRunner.addResult(`Clicking preview element`);
      prompt._eagerPreviewElement.click();

      TestRunner.addResult('Selection after: ' + window.getSelection().toString());
      TestRunner.addResult(`Editor has focus: ${editor.element.hasFocus()}`);

      next();
    },
  ]);

  async function checkExpression(expression) {
    prompt.setText(expression);
    await prompt._previewRequestForTest;
    const previewText = prompt._innerPreviewElement.deepTextContent();
    TestRunner.addResult(`Expression: "${expression}" yielded preview: "${previewText}"`);
  }

  async function evaluateAndDumpResult(expression) {
    const customFormatters = {};
    for (let name of ['objectId', 'scriptId', 'exceptionId'])
      customFormatters[name] = 'formatAsTypeNameOrNull';

    TestRunner.addResult(`Evaluating "${expression}"`);
    await TestRunner.evaluateInPage(expression, (value, exceptionDetails) => {
      TestRunner.dump(value, customFormatters, '', 'value: ');
      TestRunner.dump(exceptionDetails, customFormatters, '', 'exceptionDetails: ');
    });
  }
})();
