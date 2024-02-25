// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as TextUtils from 'devtools/models/text_utils/text_utils.js';

(async function() {
  TestRunner.addResult(`Tests how utilities functions highlight text and then revert/re-apply highlighting changes.\n`);

  function dumpTextNodesAsString(node) {
    var result = '';
    function dumpTextNode(node) {
      var str = node.textContent;
      if (node.parentElement.className)
        result += '[' + str + ']';
      else
        result += str;
    };

    function dumpElement(element) {
      for (var i = 0; i < element.childNodes.length; i++)
        dumpNode(element.childNodes[i]);
    }

    function dumpNode(node) {
      if (node.nodeType === Node.TEXT_NODE)
        dumpTextNode(node);
      else if (node.nodeType === Node.ELEMENT_NODE)
        dumpElement(node);
    };

    dumpNode(node);

    return result;
  }

  function performTestForElement(element, ranges) {
    var changes = [];
    TestRunner.addResult('--------- Running test: ----------');
    UIModule.UIUtils.highlightRangesWithStyleClass(element, ranges, 'highlighted', changes);
    TestRunner.addResult('After highlight: ' + dumpTextNodesAsString(element));
    UIModule.UIUtils.revertDomChanges(changes);
    TestRunner.addResult('After revert: ' + dumpTextNodesAsString(element));
    UIModule.UIUtils.applyDomChanges(changes);
    TestRunner.addResult('After apply: ' + dumpTextNodesAsString(element));
  }

  function textElement(strings) {
    var element = document.createElement('div');
    for (var i = 0; i < strings.length; i++) {
      var span = document.createElement('span');
      span.textContent = strings[i];
      element.appendChild(span);
    }
    return element;
  }

  function range(offset, length) {
    return new TextUtils.TextRange.SourceRange(offset, length);
  }

  performTestForElement(textElement(['function']), [range(0, 8)]);  // Highlight whole text node.
  performTestForElement(textElement(['function']), [range(0, 7)]);  // Highlight only text node beginning.
  performTestForElement(textElement(['function']), [range(1, 7)]);  // Highlight only text node ending.
  performTestForElement(textElement(['function']), [range(1, 6)]);  // Highlight in the middle of text node.

  performTestForElement(
      textElement(['function', ' ', 'functionName']), [range(0, 21)]);  // Highlight all text in 3 text nodes.
  performTestForElement(
      textElement(['function', ' ', 'functionName']),
      [range(0, 20)]);  // Highlight all text in 3 text nodes except for the last character.
  performTestForElement(
      textElement(['function', ' ', 'functionName']),
      [range(1, 20)]);  // Highlight all text in 3 text nodes except for the first character.
  performTestForElement(
      textElement(['function', ' ', 'functionName']),
      [range(1, 19)]);  // Highlight all text in 3 text nodes except for the first and the last characters.
  performTestForElement(
      textElement(['function', ' ', 'functionName']), [range(7, 3)]);  // Highlight like that "functio[n f]unctionName"

  performTestForElement(
      textElement(['function', ' ', 'functionName']),
      [range(0, 1), range(8, 1), range(9, 1)]);  // Highlight first characters in text nodes.
  performTestForElement(
      textElement(['function', ' ', 'functionName']),
      [range(7, 1), range(8, 1), range(20, 1)]);  // Highlight last characters in text node.
  performTestForElement(
      textElement(['function', ' ', 'functionName']),
      [range(0, 1), range(7, 3), range(20, 1)]);  // Highlight like that: "[f]unctio[n f]unctionNam[e]"
  TestRunner.completeTest();
})();
