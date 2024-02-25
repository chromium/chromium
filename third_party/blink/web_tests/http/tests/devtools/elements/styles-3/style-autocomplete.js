// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as ElementsModule from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(`Tests that autocompletions are computed correctly when editing the Styles pane.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #outer {--red-color: red}
      #middle {--blue-color: blue}
      </style>
      <div id="outer">
          <div id="middle">
              <div id="inner" style="color:initial;-webkit-transform: initial; transform: initial;"></div>
          </div>
      </div>
    `);

  await ElementsTestRunner.selectNodeAndWaitForStylesPromise('inner');

  var colorTreeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem('color');
  var namePrompt = new ElementsModule.StylesSidebarPane.CSSPropertyPrompt(colorTreeElement, true /* isEditingName */);
  var valuePrompt = valuePromptFor('color');

  function valuePromptFor(name) {
    var treeElement = ElementsTestRunner.getMatchedStylePropertyTreeItem(name);
    return new ElementsModule.StylesSidebarPane.CSSPropertyPrompt(treeElement, false /* isEditingName */);
  }
  TestRunner.runTestSuite([
    function testEmptyName(next) {
      testAgainstGolden(namePrompt, '', false, [], ['width'], next);
    },

    function testEmptyNameForce(next) {
      testAgainstGolden(namePrompt, '', true, ['width'], [], next);
    },

    function testSingleCharName(next) {
      testAgainstGolden(namePrompt, 'w', false, ['width'], [], next);
    },

    function testSubstringName(next) {
      testAgainstGolden(namePrompt, 'size', false, ['font-size', 'background-size', 'resize'], ['font-align'], next);
    },

    function testEmptyValue(next) {
      testAgainstGolden(valuePrompt, '', false, ['aliceblue', 'red', 'inherit'], [], next);
    },

    function testImportantDeclarationDoNotToggleOnExclamationMark(next) {
      testAgainstGolden(valuePrompt, 'red !', false, [], ['!important'], next);
    },

    function testImportantDeclaration(next) {
      testAgainstGolden(valuePrompt, 'red !i', false, ['!important'], [], next);
    },

    function testValueR(next) {
      testAgainstGolden(valuePrompt, 'R', false, ['RED', 'ROSYBROWN'], ['aliceblue', 'inherit'], next);
    },

    function testValueWithParenthesis(next) {
      testAgainstGolden(valuePrompt, 'saturate(0%)', false, [], ['inherit'], next);
    },

    function testValuePrefixed(next) {
      testAgainstGolden(
          valuePromptFor('-webkit-transform'), 'tr', false, ['translate', 'translateY', 'translate3d'],
          ['initial', 'inherit'], next);
    },

    function testValueUnprefixed(next) {
      testAgainstGolden(
          valuePromptFor('transform'), 'tr', false, ['translate', 'translateY', 'translate3d'],
          ['initial', 'inherit'], next);
    },

    function testValuePresets(next) {
      testAgainstGolden(
          valuePromptFor('transform'), 'tr', false, [], [], next,
          ['translate(10px, 10px)', 'translateY(10px)', 'translate3d(10px, 10px, 10px)']);
    },

    function testNameValuePresets(next) {
      testAgainstGolden(namePrompt, 'underli', false, ['text-decoration: underline'], [], next);
    },

    function testNameValuePresetWithNameMatch(next) {
      testAgainstGolden(namePrompt, 'display', false, ['display: block'], [], next);
    },

    function testValueSubstring(next) {
      testAgainstGolden(
          valuePromptFor('color'), 'blue', false, ['blue', 'darkblue', 'lightblue'],
          ['darkred', 'yellow', 'initial', 'inherit'], next);
    },

    function testNameVariables(next) {
      testAgainstGolden(namePrompt, '', true, ['--red-color', '--blue-color'], [], next);
    },

    function testValueVariables(next) {
      testAgainstGolden(valuePromptFor('color'), 'var(', true, ['--red-color', '--blue-color'], ['width'], next,
          ['--red-color)', '--blue-color)']);
    }
  ]);

  function testAgainstGolden(prompt, inputText, force, golden, antiGolden, callback, transformedGolden = []) {
    var proxyElement = document.createElement('div');
    document.body.appendChild(proxyElement);
    proxyElement.style = 'webkit-user-select: text; -webkit-user-modify: read-write-plaintext-only';
    proxyElement.textContent = inputText;
    var selectionRange = document.createRange();
    var textNode = proxyElement.childNodes[0];
    if (textNode) {
      selectionRange.setStart(textNode, inputText.length);
      selectionRange.setEnd(textNode, inputText.length);
    } else {
      selectionRange.selectNodeContents(proxyElement);
    }
    var range = Platform.DOMUtilities.rangeOfWord(selectionRange.startContainer, selectionRange.startOffset, prompt.completionStopCharacters, proxyElement, 'backward');
    var prefix = range.toString();
    prompt.buildPropertyCompletions(inputText.substring(0, inputText.length - prefix.length), prefix, force)
        .then(completions);

    function completions(result) {
      var suggestions = new Set(result.map(s => s.title || s.text));
      var appliedSuggestions = new Set(result.map(s => s.text));
      var i;
      for (i = 0; i < golden.length; ++i) {
        if (!suggestions.has(golden[i]))
          TestRunner.addResult('NOT FOUND: ' + golden[i]);
      }
      for (i = 0; i < antiGolden.length; ++i) {
        if (suggestions.has(antiGolden[i]))
          TestRunner.addResult('FOUND: ' + antiGolden[i]);
      }
      for (i = 0; i < transformedGolden.length; ++i) {
        if (!appliedSuggestions.has(transformedGolden[i]))
          TestRunner.addResult('NOT FOUND: ' + transformedGolden[i]);
      }
      proxyElement.remove();
      callback();
    }
  }
})();
