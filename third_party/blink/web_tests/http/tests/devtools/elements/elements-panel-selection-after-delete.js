// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as ElementsModule from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(`Tests that elements panel correctly updates selection on node removal.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <head>
        <script>
        function prepareTestTree()
        {
            var template = document.querySelector("#testTree");
            var testTreeContainer = document.querySelector("#testTreeContainer");
            testTreeContainer.textContent = "";
            testTreeContainer.appendChild(document.importNode(template.content, true));
        }
        </script>
      </head>
      <body>
      <template id="testTree">
          <div class="left">
              <div class="child1">
              </div>
              <div class="child2">
                  <div class="child5">
                  </div>
                  <div class="child6">
                  </div>
                  <div class="child7">
                  </div>
                  <div class="child8">
                  </div>
              </div>
              <div class="child3">
              </div>
          </div>
      </template>
      <div id="testTreeContainer">
      </div>
      </body>
    `);

  function selectNode(className, callback) {
    var selector = '#testTreeContainer .' + className;
    ElementsTestRunner.querySelector(selector, gotNode);

    function gotNode(node) {
      ElementsTestRunner.selectNode(node).then(callback);
    }
  }

  function nodeToString(node) {
    if (!node)
      return 'null';
    var result = '<' + node.nodeName();
    var attributes = node.attributes();
    for (var attribute of attributes) {
      result += ' ' + attribute.name;
      if (attribute.value)
        result += '="' + attribute.value + '"';
    }
    result += '>';
    return result;
  }

  function prepareTestTree(callback) {
    TestRunner.evaluateInPage('prepareTestTree()', callback);
  }

  function removeElementAsUser(element, callback) {
    TestRunner.addSniffer(ElementsModule.ElementsTreeOutline.ElementsTreeOutline.prototype, 'updateModifiedNodes', callback);
    element.remove();
  }

  function removeElementExternally(element, callback) {
    var node = element.node();
    TestRunner.addSniffer(ElementsModule.ElementsTreeOutline.ElementsTreeOutline.prototype, 'updateChildren', callback);
    node.removeNode();
  }

  function dumpSelectedNode() {
    var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
    var selectedNode = selectedElement ? selectedElement.node() : null;
    TestRunner.addResult('The following node is now selected: ' + nodeToString(selectedNode));
  }

  TestRunner.runTestSuite([
    function testUserDelete(next) {
      prepareTestTree(step2);

      function step2() {
        var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
        treeOutline.runPendingUpdates();
        TestRunner.addResult('Selecting node...');
        selectNode('child2', step3);
      }

      function step3() {
        dumpSelectedNode();

        TestRunner.addResult('Deleting selected node like it was a user action...');
        var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
        removeElementAsUser(selectedElement, step4);
      }

      function step4() {
        dumpSelectedNode();

        TestRunner.addResult('Deleting selected node like it was a user action again...');
        var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
        removeElementAsUser(selectedElement, step5);
      }

      function step5() {
        dumpSelectedNode();

        TestRunner.addResult('Deleting last child like it was a user action...');
        var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
        removeElementAsUser(selectedElement, step6);
      }

      function step6() {
        dumpSelectedNode();
        next();
      }
    },

    function testExternalDelete(next) {
      prepareTestTree(step2);

      function step2() {
        var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
        treeOutline.runPendingUpdates();
        TestRunner.addResult('Selecting node...');
        selectNode('child2', step3);
      }

      function step3() {
        dumpSelectedNode();

        TestRunner.addResult('Deleting selected node externally...');
        var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
        removeElementExternally(selectedElement, step4);
      }

      function step4() {
        dumpSelectedNode();

        TestRunner.addResult('Deleting selected node externally again...');
        var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
        removeElementExternally(selectedElement, step5);
      }

      function step5() {
        dumpSelectedNode();

        TestRunner.addResult('Deleting last child externally...');
        var selectedElement = ElementsTestRunner.firstElementsTreeOutline().selectedTreeElement;
        removeElementExternally(selectedElement, step6);
      }

      function step6() {
        dumpSelectedNode();
        next();
      }
    },
  ]);
})();
