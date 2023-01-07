// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>

      body {
          margin: 0;
      }

      iframe {
          position: absolute;
          left: 83px;
          top: 53px;
          width: 200px;
          height: 200px;
      }

      </style>
      <iframe id="transform-iframe" src="resources/highlight-node-transformed-iframe.html"></iframe>
    `);

  ElementsTestRunner.dumpInspectorHighlightJSON('div', TestRunner.completeTest.bind(TestRunner));
})();
