// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that user can mutate DOM by means of elements panel.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('text_editor');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div>
          <div id="testEditCommentAsHTML">
              <!-- Comment -->
          </div>

          <div id="testEditAsHTML">
              <div id="node-to-edit-as-html"><span id="span">Text</span></div>
          </div>
      </div>
    `);

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(next);
    },

    function testEditCommentAsHTML(next) {
      function commentNodeSelectionCallback(testNode, callback) {
        var childNodes = testNode.children();
        for (var i = 0; i < childNodes.length; ++i) {
          if (childNodes[i].nodeType() === 8) {
            Common.Revealer.reveal(childNodes[i]);
            callback(childNodes[i]);
            return;
          }
        }
        TestRunner.addResult('Comment node not found');
        TestRunner.completeTest();
      }
      ElementsTestRunner.domActionTest('testEditCommentAsHTML', commentNodeSelectionCallback, testBody, next);

      function testBody(node, done) {
        var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
        var treeElement = treeOutline.findTreeElement(node);
        treeOutline.toggleEditAsHTML(node);
        TestRunner.deprecatedRunAfterPendingDispatches(step2);

        function step2() {
          TestRunner.addResult(treeElement._editing.editor.text());
          treeElement._editing.editor.setText('<div foo="bar-comment">Element</div>');
          var event = TestRunner.createKeyEvent('Enter');
          event.isMetaOrCtrlForTest = true;
          treeElement._editing.editor.widget().element.dispatchEvent(event);
          TestRunner.deprecatedRunAfterPendingDispatches(done);
        }
      }
    },

    function testEditAsHTML(next) {
      ElementsTestRunner.domActionTestForNodeId('testEditAsHTML', 'node-to-edit-as-html', testBody, next);

      function testBody(node, done) {
        var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
        var treeElement = treeOutline.findTreeElement(node);
        treeOutline.toggleEditAsHTML(node);
        TestRunner.deprecatedRunAfterPendingDispatches(step2);

        function step2() {
          TestRunner.addResult(treeElement._editing.editor.text());
          treeElement._editing.editor.setText('<span foo="bar"><span id="inner-span">Span contents</span></span>');
          var event = TestRunner.createKeyEvent('Enter');
          event.isMetaOrCtrlForTest = true;
          treeElement._editing.editor.widget().element.dispatchEvent(event);
          TestRunner.deprecatedRunAfterPendingDispatches(
              ElementsTestRunner.expandElementsTree.bind(ElementsTestRunner, done));
        }
      }
    },
  ]);
})();
