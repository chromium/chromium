// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that oopif iframes are rendered inline.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  await TestRunner.navigatePromise('resources/page.html');

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(next);
    },

    function testRemove(next) {
      ElementsTestRunner.dumpElementsTree();
      TestRunner.completeTest();
    },
  ]);
})();
