// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `The test verifies that SelectedNodeChanged event is not fired whenever a child gets added to the node.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div>
          <div id="first">First Child</div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function appendNewNode()
      {
          var element = document.querySelector("#first");
          var second = document.createElement("div");
          element.parentElement.appendChild(second);
      }
  `);

  ElementsTestRunner.selectNodeWithId('first', onNodeSelected);

  function onNodeSelected() {
    ElementsTestRunner.firstElementsTreeOutline().addEventListener(
        Elements.ElementsTreeOutline.Events.SelectedNodeChanged, onSelectionChangedEvent);
    TestRunner.addSniffer(Elements.ElementsTreeOutline.prototype, 'updateChildren', onNodeAppended);
    TestRunner.evaluateInPage('appendNewNode()');
  }

  function onSelectionChangedEvent() {
    TestRunner.addResult('ERROR: erroneous selection changed event received.');
    TestRunner.completeTest();
  }

  function onNodeAppended() {
    TestRunner.completeTest();
  }
})();
