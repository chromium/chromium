// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that adding @import rules into a stylesheet through JavaScript does not crash the inspected page.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      </style>
      <div>
          <p id="inspected"></p>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function addImportRule()
      {
          document.styleSheets[0].insertRule("@import url(resources/import-added-through-js-crash.css)", 0);
      }
  `);

  TestRunner.runTestSuite([
    function selectNode(next) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', next);
    },

    function addImportRules(next) {
      ElementsTestRunner.waitForStyles('inspected', callback);
      TestRunner.evaluateInPage('addImportRule()');

      function callback() {
        ElementsTestRunner.waitForStyles('inspected', next);
        TestRunner.evaluateInPage('addImportRule()');
      }
    }
  ]);
})();
