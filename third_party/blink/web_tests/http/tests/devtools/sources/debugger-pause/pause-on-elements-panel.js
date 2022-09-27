// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that debugger pause button works on Elements panel after a DOM node highlighting. Chromium bug 433366\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div id="test"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function callback()
      {
          return 0;
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    UI.inspectorView.showPanel('sources').then(step2);
  }

  function step2() {
    ElementsTestRunner.nodeWithId('test', step3);
  }

  function step3(node) {
    TestRunner.assertTrue(!!node);
    TestRunner.evaluateInPage(
        'setTimeout(callback, 200)', step4.bind(null, node));
  }

  function step4(node) {
    TestRunner.assertTrue(!UI.panels.sources.paused());
    SourcesTestRunner.togglePause();

    // This used to skip the pause request (the test used to timeout).
    node.highlight();

    SourcesTestRunner.waitUntilPaused(step5);
  }

  async function step5(callFrames) {
    await SourcesTestRunner.captureStackTrace(callFrames);
    TestRunner.addResult('PASS: Debugger paused.');
    SourcesTestRunner.completeDebuggerTest();
  }
})();
