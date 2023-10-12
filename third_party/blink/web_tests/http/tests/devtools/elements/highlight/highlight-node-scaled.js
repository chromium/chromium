// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`\n`);
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
      <iframe id="scale-iframe" src="resources/highlight-node-scaled-iframe.html"></iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      if (window.internals)
          internals.setPageScaleFactor(2);
  `);

  ElementsTestRunner.dumpInspectorHighlightJSON('div', TestRunner.completeTest.bind(TestRunner));
})();
