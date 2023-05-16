// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that selector line is computed correctly regardless of its start column. Bug 110732.\n`);
  await TestRunner.loadLegacyModule('elements');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected
      {
        color: green;
      }
      </style>
      <div id="container">
          <div id="inspected">Text</div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function addStylesheet()
      {
          var linkElement = document.createElement("link");
          linkElement.rel = "stylesheet";
          linkElement.href = "resources/selector-line-deprecated.css";
          document.head.appendChild(linkElement);
      }
  `);

  TestRunner.evaluateInPage('addStylesheet()');
  TestRunner.addSniffer(Bindings.CSSWorkspaceBinding.prototype, 'updateLocations', sourceMappingSniffer, true);

  function sourceMappingSniffer(header) {
    if (header.resourceURL().includes('selector-line-deprecated.css')) {
      ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step2);
    }
  }

  async function step2() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
