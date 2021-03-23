// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that the console enters a newline instead of running a command if the command is incomplete.\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  var prompt = Console.ConsoleView.instance()._prompt;
  ConsoleTestRunner.waitUntilConsoleEditorLoaded().then(step1);

  function step1() {
    sequential([
      () => pressEnterAfter('window'),
      () => pressEnterAfter('window.'),
      () => pressEnterAfter('if (1 === 2)'),
      () => pressEnterAfter('if (1 === 2) {'),
      () => pressEnterAfter('if (1 === 2) {}'),
      () => pressEnterAfter('[1,2,'),
      () => pressEnterAfter('[1,2,3]'),
      () => pressEnterAfter('{abc:'),
      () => pressEnterAfter('{abc:123}'),
      () => pressEnterAfter(`function incomplete() {
    if (1)
        5;`),
      () => pressEnterAfter(`function bad() {
    if (1)
}`),
      () => pressEnterAfter(`function good() {
    if (1) {
        5;
    }
}`),
      () => pressEnterAfter('1,'),
      () => pressEnterAfter('label:'),
      () => pressEnterAfter('for (var i = 0; i < 100; i++)'),
      () => pressEnterAfter('for (var i = 0; i < 100; i++) i'),
      () => pressEnterAfter('var templateStr = `'),
      () => pressEnterAfter('var templateStr = `str`'),
      () => pressEnterAfter('var doubleQ = "'),
      () => pressEnterAfter('var singleQ = \''),
      () => pressEnterAfter('var singleQ = \'str\'')
    ]).then(TestRunner.completeTest);
  }

  function sequential(tests) {
    var promise = Promise.resolve();
    for (var i = 0; i < tests.length; i++)
      promise = promise.then(tests[i]);
    return promise;
  }

  function pressEnterAfter(text) {
    var fulfill;
    var promise = new Promise(x => fulfill = x);
    TestRunner.addSniffer(Console.ConsolePrompt.prototype, '_enterProcessedForTest', enterProcessed);

    prompt.setText(text);
    prompt.moveCaretToEndOfPrompt();
    prompt._enterKeyPressed(TestRunner.createKeyEvent('Enter'));
    return promise;

    function enterProcessed() {
      TestRunner.addResult('Text Before Enter:');
      TestRunner.addResult(text.replace(/ /g, '.'));
      TestRunner.addResult('Text After Enter:');
      TestRunner.addResult(prompt.text().replace(/ /g, '.') || '<empty>');
      TestRunner.addResult('');
      fulfill();
    }
  }
})();
