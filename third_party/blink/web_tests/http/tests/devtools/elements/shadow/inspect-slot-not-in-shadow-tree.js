// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that slots that are not in a shadow tree can be inspected.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p id="description"></p>

      <slot id="test1"><span>test</span></slot>
    `);

  TestRunner.overlayModel.setInspectMode(Protocol.Overlay.InspectMode.SearchForNode).then(finishTest);
  ConsoleTestRunner.evaluateInConsole('inspect(test1)');
  function finishTest() {
    TestRunner.addResult('Inspect successful');
    TestRunner.completeTest();
  }
})();
