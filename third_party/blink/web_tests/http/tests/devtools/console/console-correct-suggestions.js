// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as TextUtils from 'devtools/models/text_utils/text_utils.js';

(async function() {
  TestRunner.addResult(`Tests that console correctly finds suggestions in complicated cases.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
     function templateString()
    {
        console.log("The template string should not run and you should not see this log");
        return {
            shouldNotFindThis:56
        };
    }
    function shouldNotFindThisFunction() { }
    function shouldFindThisFunction() { }
    window["should not find this"] = true;
    window._foo = true;
    var myMap = new Map([['first', 1], ['second', 2], ['third', 3], ['shouldNotFindThis', 4]]);
    var complicatedObject = {
        'foo-bar': true,
        '"double-qouted"': true,
        "'single-qouted'": true,
        "notDangerous();": true
    }
    var cantTouchThis = false;
    function dontRunThis() {
      cantTouchThis = true;
    }
    function stall() {
      while(true);
    }
    var thePrefix = true;
    var thePrefixAndTheSuffix = true;
    class ClassWithMethod {
        method(){}
    }
    const objWithMethod = new ClassWithMethod();
    objWithMethod.methodWithSuffix = true;
  `);

  var consoleEditor;

  /**
   * @param {string} text
   * @param {!Array<string>} expected
   * @param {boolean=} force
   * @param {boolean=} reportDefault
   */
  async function testCompletions(text, expected, force, reportDefault) {
    var cursorPosition = text.indexOf('|');

    if (cursorPosition < 0)
      cursorPosition = Infinity;

    consoleEditor.setText(text.replace('|', ''));
    consoleEditor.setSelection(TextUtils.TextRange.TextRange.createFromLocation(0, cursorPosition));
    consoleEditor.autocompleteController.autocomplete(force);
    var message =
        'Checking \'' + text.replace('\n', '\\n').replace('\r', '\\r') + '\'';

    if (force)
      message += ' forcefully';

    TestRunner.addResult(message);
    const suggestions = await TestRunner.addSnifferPromise(
        consoleEditor.autocompleteController, '_onSuggestionsShownForTest');
    var completions =
        new Map(suggestions.map(suggestion => [suggestion, suggestion.text]))
            .inverse();

    for (const suggestionText of completions.keysArray()) {
      const size = completions.get(suggestionText).size;
      if (size > 1)
        TestRunner.addResult(`ERROR! ${size} duplicates found for '${suggestionText}'!`)
    }

    for (var i = 0; i < expected.length; i++) {
      if (completions.has(expected[i])) {
        for (var completion of completions.get(expected[i])) {
          if (completion.title)
            TestRunner.addResult(
                'Found: ' + expected[i] + ', displayed as ' + completion.title);
          else
            TestRunner.addResult('Found: ' + expected[i]);
        }
      } else {
        TestRunner.addResult('Not Found: ' + expected[i]);
      }
    }

    if (reportDefault) {
        const defaultSuggestion = suggestions.reduce((a, b) => (a.priority || 0) >= (b.priority || 0) ? a : b);
        if (defaultSuggestion.title)
            TestRunner.addResult(`Default suggestion: ${defaultSuggestion.text}, displayed as ${defaultSuggestion.title}`);
        else
            TestRunner.addResult(`Default suggestion: ${defaultSuggestion.text}`);
    }

    if (await TestRunner.evaluateInPagePromise('cantTouchThis') !== false) {
      TestRunner.addResult('ERROR! Side effects were detected!');
      await TestRunner.evaluateInPagePromise('cantTouchThis = false');
    }

    TestRunner.addResult('');
  }

  function sequential(tests) {
    var promise = Promise.resolve();

    for (var i = 0; i < tests.length; i++)
      promise = promise.then(tests[i]);

    return promise;
  }

  sequential([
    () => ConsoleTestRunner.waitUntilConsoleEditorLoaded().then(
        e => consoleEditor = e),
    () => testCompletions('window.|foo', ['_foo']),
    () => testCompletions('window._|foo', ['_foo'], false),
    () => testCompletions('window._|foo', ['_foo'], true),
    () => testCompletions('window.do', ['document']),
    () => testCompletions('win', ['window']),
    () => testCompletions('window["doc', ['"document"]']),
    () => testCompletions('window["document"].bo', ['body']),
    () => testCompletions('window["document"]["body"].textC', ['textContent']),
    () => testCompletions('document.body.inner', ['innerText', 'innerHTML']),
    () => testCompletions('document["body"][window.do', ['document']),
    () => testCompletions(
        'document["body"][window["document"].body.childNodes[0].text',
        ['textContent']),
    () => testCompletions('templateString`asdf`should', ['shouldNotFindThis']),
    () => testCompletions('window.document.BODY', ['body']),
    () => testCompletions('window.dOcUmE', ['document']),
    () => testCompletions('window.node', ['NodeList', 'AudioNode', 'GainNode']),
    () => testCompletions('32', ['Float32Array', 'Int32Array']),
    () => testCompletions('window.32', ['Float32Array', 'Int32Array']),
    () => testCompletions('', ['window'], false),
    () => testCompletions('', ['window'], true),
    () => testCompletions('"string g', ['getComputedStyle'], false),
    () => testCompletions('`template string docu', ['document'], false),
    () => testCompletions('`${do', ['document'], false),
    () => testCompletions('// do', ['document'], false),
    () => testCompletions('["should', ['shouldNotFindThisFunction']),
    () => testCompletions('shou', ['should not find this']),
    () => testCompletions('myMap.get(', ['"first")', '"second")', '"third")']),
    () => testCompletions(
        'myMap.get(\'', ['\'first\')', '\'second\')', '\'third\')']),
    () => testCompletions('myMap.set(\'firs', ['\'first\', ']),
    () => testCompletions(
        'myMap.set(should',
        [
          'shouldFindThisFunction', 'shouldNotFindThis', '"shouldNotFindThis")'
        ]),
    () => testCompletions(
        'myMap.delete(\'', ['\'first\')', '\'second\')', '\'third\')']),
    () => testCompletions('document.   bo', ['body']),
    () => testCompletions('document.\tbo', ['body']),
    () => testCompletions('document.\nbo', ['body']),
    () => testCompletions('document.\r\nbo', ['body']),
    () => testCompletions('document   [    \'bo', ['\'body\']']),
    () => testCompletions('function hey(should', ['shouldNotFindThisFunction']),
    () => testCompletions('var should', ['shouldNotFindThisFunction']),
    () => testCompletions('document[[win', ['window']),
    () => testCompletions('document[   [win', ['window']),
    () => testCompletions('document[   [  win', ['window']),
    () => testCompletions('I|mag', ['Image', 'Infinity']),
    () => testCompletions('I|mage', ['Image', 'Infinity'], false),
    () => testCompletions('I|mage', ['Image', 'Infinity'], true),
    () => testCompletions('var x = (do|);', ['document']),
    () => testCompletions('complicatedObject["foo', ['"foo-bar"]']),
    () => testCompletions('complicatedObject["foo-', ['"foo-bar"]']),
    () => testCompletions('complicatedObject["foo-bar', ['"foo-bar"]']),
    () =>
        testCompletions('complicatedObject["\'sing', ['"\'single-qouted\'"]']),
    () => testCompletions(
        'complicatedObject[\'\\\'sing', ['\'\\\'single-qouted\\\'\']']),
    () => testCompletions(
        'complicatedObject["\'single-qou', ['"\'single-qouted\'"]']),
    () => testCompletions(
        'complicatedObject["\\"double-qouted\\"', ['"\\"double-qouted\\""]']),
    () => testCompletions(
        'complicatedObject["notDangerous();', ['"notDangerous();"]']),
    () => testCompletions('queryOb', ['queryObjects']),
    () => testCompletions('fun', ['function']),
    () => testCompletions(
        '"stringwithscary!@#$%^&*()\\"\'`~+-=".char', ['charAt']),
    () => testCompletions('("hello" + "goodbye").char', ['charAt']),
    () => testCompletions('({7: "string"})[3 + 4].char', ['charAt']),
    () => testCompletions('cantTouchThis++; "string".char', ['charAt']),
    () => testCompletions('cantTouchThis++ + "string".char', ['charAt']),
    () => testCompletions('var x = "string".char', ['charAt']),
    () => testCompletions('({abc: 123}).a', ['abc']),
    () => testCompletions('{dontFindLabels: 123}.dont', ['dontFindLabels']),
    () => testCompletions(
        'const x = 5; {dontFindLabels: 123}.dont', ['dontFindLabels']),
    () => testCompletions('const x = {abc: 123}.a', ['abc']),
    () => testCompletions('x = {abc: 123}.', ['abc']),
    () => testCompletions('[1,2,3].j', ['join']),
    () => testCompletions('document.body[{abc: "children"}.abc].', ['length']),
    () => testCompletions('new Date.', ['UTC', 'toUTCString']),
    () => testCompletions('new Date().', ['UTC', 'toUTCString']),
    () => testCompletions('const x = {7: "hi"}[3+4].', ['anchor']),
    () => testCompletions('["length"]["length"].', ['toExponential']),
    () => testCompletions('(await "no completions").', ['toString']),
    () => testCompletions('(++cantTouchThis).', []),
    () => testCompletions('(cantTouchThis -= 2).', []),
    () => testCompletions('new dontRunThis.', []),
    () => testCompletions('new dontRunThis().', []),
    () => testCompletions('(new dontRunThis).', []),
    () => testCompletions('(dontRunThis`asdf`).', []),
    () => testCompletions('dontRunThis().', []),
    () => testCompletions('stall().', []),
    () => testCompletions(
        'shouldNot|FindThisFunction()', ['shouldNotFindThisFunction']),
    () => testCompletions('thePrefix', ['thePrefix', 'thePrefixAndTheSuffix']),
    () => testCompletions('objWithMethod.method', ['method', 'methodWithSuffix'], false, true),
  ]).then(TestRunner.completeTest);
})();
