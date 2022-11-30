// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test verifies that template's content DocumentFragment is accessible from DevTools.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p id="description"></p>

      <template id="tpl">
          <div>Hello!</div>
      </template>
    `);

  ElementsTestRunner.expandElementsTree(function() {
    var contentNode = ElementsTestRunner.expandedNodeWithId('tpl').templateContent();
    UI.panels.elements.selectDOMNode(contentNode, true);
    ConsoleTestRunner.evaluateInConsole('$0', callback);
  });

  function callback(result) {
    TestRunner.addResult('SUCCESS');
    TestRunner.completeTest();
  }
})();
