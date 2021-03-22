// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that user can mutate DOM by means of elements panel.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
<div>
    <div id="testEditInvisibleCharsAsHTML">
        <div id="node-with-invisible-chars">A&#xA0;B&#x2002;C&#x2003;D&#x2009;E&#x200C;F&#x200D;G&#x200F;H&#x200E;I</div>
    </div>

    <div id="testEditScript">
      <script id="node-to-edit-script">

          var i = 0;
          var j = 5;
          for (; i < j; ++i) {
              // Do nothing.
          }

      </script>
    </div>

    <div id="testEditSVG">
        <svg id="node-to-edit-svg-attribute" xmlns:xlink="test" width="100">
        </svg>
    </div>
</div>
    `);

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(next);
    },

    function testEditInvisibleCharsAsHTML(next) {
      ElementsTestRunner.domActionTestForNodeId(
          'testEditInvisibleCharsAsHTML', 'node-with-invisible-chars', testBody, next);

      function testBody(node, done) {
        var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
        var treeElement = treeOutline.findTreeElement(node);
        treeOutline.toggleEditAsHTML(node);
        TestRunner.deprecatedRunAfterPendingDispatches(step2);

        function step2() {
          var editor = treeElement._editing.editor;
          TestRunner.addResult(editor.text());
          editor.setText(editor.text().replace(/&/g, '#'));
          var event = TestRunner.createKeyEvent('Enter');
          event.isMetaOrCtrlForTest = true;
          editor.widget().element.dispatchEvent(event);
          TestRunner.deprecatedRunAfterPendingDispatches(
              ElementsTestRunner.expandElementsTree.bind(ElementsTestRunner, done));
        }
      }
    },

    function testEditScript(next) {
      ElementsTestRunner.domActionTestForNodeId('testEditScript', 'node-to-edit-script', testBody, next);

      function testBody(node, done) {
        ElementsTestRunner.editNodePartAndRun(node, 'webkit-html-text-node', 'var i = 0;\n    var j = 0;\n', done);
      }
    },

    function testEditSVGAttribute(next) {
      ElementsTestRunner.domActionTestForNodeId('testEditSVG', 'node-to-edit-svg-attribute', testBody, next);

      function testBody(node, done) {
        var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
        var treeElement = treeOutline.findTreeElement(node);
        treeOutline.toggleEditAsHTML(node);
        TestRunner.deprecatedRunAfterPendingDispatches(step2);

        function step2() {
          var value = treeElement._editing.editor._codeMirror.getValue();
          TestRunner.addResult(value);
          treeElement._editing.editor.setText(value.replace('100', '110'));
          var event = TestRunner.createKeyEvent('Enter');
          event.isMetaOrCtrlForTest = true;
          treeElement._editing.editor.widget().element.dispatchEvent(event);
          TestRunner.deprecatedRunAfterPendingDispatches(
              ElementsTestRunner.expandElementsTree.bind(ElementsTestRunner, done));
        }
      }
    }
  ]);
})();
