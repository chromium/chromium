// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Tests DOM update highlights in the DOM tree.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container">
          <div id="attrTest" attrfoo="foo"></div>
          <div id="childTest"></div>
          <div id="textTest"></div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function appendChild(parentId, id)
      {
          var e = document.createElement("span");
          e.id = id;
          document.getElementById(parentId).appendChild(e);
      }

      function remove(id)
      {
          document.getElementById(id).remove();
      }

      function removeFirstChild(id)
      {
          document.getElementById(id).firstChild.remove();
      }

      function setAttribute(id, name, value)
      {
          var e = document.getElementById(id);
          if (value === undefined)
              e.removeAttribute(name);
          else
              e.setAttribute(name, value);
      }

      function setTextContent(id, content)
      {
          document.getElementById(id).textContent = content;
      }

      function setFirstChildTextContent(id, content)
      {
          document.getElementById(id).firstChild.textContent = content;
      }
  `);

  var attrTestNode;
  var childTestNode;
  var textTestNode;

  TestRunner.runTestSuite([
    function testDumpInitial(next) {
      function callback(node) {
        attrTestNode = ElementsTestRunner.expandedNodeWithId('attrTest');
        childTestNode = ElementsTestRunner.expandedNodeWithId('childTest');
        textTestNode = ElementsTestRunner.expandedNodeWithId('textTest');
        next();
      }
      TestRunner.addResult('========= Original ========');
      ElementsTestRunner.dumpDOMUpdateHighlights(null);
      ElementsTestRunner.expandElementsTree(callback);
    },

    function testSetAttributeOtherValue(next) {
      runAndDumpHighlights('setAttribute(\'attrTest\', \'attrFoo\', \'bar\')', attrTestNode, next);
    },

    function testSetAttributeEmpty(next) {
      runAndDumpHighlights('setAttribute(\'attrTest\', \'attrFoo\', \'\')', attrTestNode, next);
    },

    function testAddAttribute(next) {
      runAndDumpHighlights('setAttribute(\'attrTest\', \'attrBar\', \'newBar\')', attrTestNode, next);
    },

    function testRemoveAttribute(next) {
      runAndDumpHighlights('setAttribute(\'attrTest\', \'attrFoo\')', attrTestNode, next);
    },

    function testAppendChildToEmpty(next) {
      runAndDumpHighlights('appendChild(\'childTest\', \'child1\')', childTestNode, callback);
      function callback() {
        // Expand the #childTest node.
        ElementsTestRunner.expandElementsTree(next);
      }
    },

    function testAppendChildToExpanded(next) {
      runAndDumpHighlights('appendChild(\'childTest\', \'child2\')', childTestNode, next);
    },

    function testRemoveChild1(next) {
      runAndDumpHighlights('remove(\'child1\')', childTestNode, next);
    },

    function testRemoveChild2(next) {
      runAndDumpHighlights('remove(\'child2\')', childTestNode, next);
    },

    function testSetTextContent(next) {
      runAndDumpHighlights('setTextContent(\'textTest\', \'Text\')', textTestNode, next);
    },

    function testSetTextNodeTextContent(next) {
      runAndDumpHighlights('setFirstChildTextContent(\'textTest\', \'NewText\')', textTestNode, next);
    },

    function testRemoveInlineTextNode(next) {
      runAndDumpHighlights('removeFirstChild(\'textTest\')', textTestNode, next);
    },

    function testSetTextContentWithEmptyText(next) {
      runAndDumpHighlights('setTextContent(\'textTest\', \'Text\')', textTestNode, next);
    },

    function testClearTextNodeTextContent(next) {
      runAndDumpHighlights('setFirstChildTextContent(\'textTest\', \'\')', textTestNode, next);
    },

    async function testAppendChildWhenHidden(next) {
      await UI.ViewManager.ViewManager.instance().showView('console');
      runAndDumpHighlights('appendChild(\'childTest\', \'child1\')', childTestNode, next);
    }
  ]);

  function runAndDumpHighlights(script, root, next) {
    dumpHighlights(root, next);
    TestRunner.evaluateInPage(script);
  }

  function dumpHighlights(root, next) {
    ElementsTestRunner.dumpDOMUpdateHighlights(root, callback);

    function callback() {
      var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
      var highlights = treeOutline.elementInternal.getElementsByClassName('dom-update-highlight');
      for (var i = 0; i < highlights.length; ++i)
        highlights[i].classList.remove('dom-update-highlight');
      next();
    }
  }
})();
