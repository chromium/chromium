// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that user can mutate author shadow DOM by means of elements panel.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div>
          <div id="testSetAuthorShadowDOMElementAttribute"></div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function createRootWithContents(id, html)
      {
          var container = document.getElementById(id);
          var root = container.attachShadow({mode: 'open'});
          root.innerHTML = html;
      }

      createRootWithContents("testSetAuthorShadowDOMElementAttribute", "<div foo='attribute value' id='shadow-node-to-set-attribute'></div>");
    `);

  // Save time on style updates.
  await UI.viewManager.showView('elements');

  ElementsTestRunner.ignoreSidebarUpdates();

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(next);
    },

    function testSetAuthorShadowDOMElementAttribute(next) {
      ElementsTestRunner.domActionTestForNodeId(
          'testSetAuthorShadowDOMElementAttribute', 'shadow-node-to-set-attribute', testBody, next);

      function testBody(node, done) {
        ElementsTestRunner.editNodePartAndRun(node, 'webkit-html-attribute', 'bar="edited attribute"', done, true);
      }
    },
  ]);
})();
