// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that CPU profiling works. https://bugs.webkit.org/show_bug.cgi?id=52634\n');
  await TestRunner.loadTestModule('cpu_profiler_test_runner');
  await TestRunner.showPanel('js_profiler');

  CPUProfilerTestRunner.runProfilerTestSuite([async function testProfiling(next) {

    CPUProfilerTestRunner.showProfileWhenAdded('profile');
    CPUProfilerTestRunner.waitUntilProfileViewIsShown('profile', findPageFunctionInProfileView);

    await TestRunner.evaluateInPagePromise(`
        function pageFunction() {
          console.profile("profile");
          console.profileEnd("profile");
        }
        pageFunction();`);

    async function checkFunction(tree, name) {
      let node = tree.children[0];
      if (!node)
        TestRunner.addResult('no node');
      while (node) {
        // Ordering is important here, as accessing the element the first time around
        // triggers live location creation and updates which we need to await properly.
        const element = node.element();
        await TestRunner.waitForPendingLiveLocationUpdates();
        const url = element.children[2].lastChild.textContent;
        if (node.functionName === name) {
          TestRunner.addResult(`found ${name} ${url}`);
          return;
        }
        node = node.traverseNextNode(true, null, true);
      }
      TestRunner.addResult(name + ' not found');
    }

    async function findPageFunctionInProfileView(view) {
      const tree = view.profileDataGridTree;
      if (!tree)
        TestRunner.addResult('no tree');
      await checkFunction(tree, 'pageFunction');
      await checkFunction(tree, '(anonymous)');
      next();
    }
  }]);

})();
