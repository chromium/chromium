// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that user can mutate author shadow DOM by means of elements panel.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('text_editor');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div>
          <div id="testEditAuthorShadowDOMAsHTML"></div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function createRootWithContents(id, html)
      {
          var container = document.getElementById(id);
          var root = container.attachShadow({mode: 'open'});
          root.innerHTML = html;
      }

      createRootWithContents("testEditAuthorShadowDOMAsHTML", "<div id='authorShadowDOMElement'></div>");
    `);

  // Save time on style updates.
  await UI.viewManager.showView('elements');

  ElementsTestRunner.ignoreSidebarUpdates();

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(next);
    },

    function testEditShadowDOMAsHTML(next) {
      ElementsTestRunner.domActionTestForNodeId(
          'testEditAuthorShadowDOMAsHTML', 'authorShadowDOMElement', testBody, next);

      function testBody(node, done) {
        var treeOutline = ElementsTestRunner.firstElementsTreeOutline();
        var treeElement = treeOutline.findTreeElement(node);
        treeOutline.toggleEditAsHTML(node);
        TestRunner.deprecatedRunAfterPendingDispatches(step2);

        function step2() {
          TestRunner.addResult(treeElement._editing.editor.text());
          treeElement._editing.editor.setText(
              '<span foo="shadow-span"><span id="inner-shadow-span">Shadow span contents</span></span>');
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
