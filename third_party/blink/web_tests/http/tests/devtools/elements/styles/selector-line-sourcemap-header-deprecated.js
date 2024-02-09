// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as BindingsModule from 'devtools/models/bindings/bindings.js';

(async function() {
  TestRunner.addResult(`Tests that sourcemap is applied correctly when specified by the respective HTTP header.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container">
          <div id="inspected">Text</div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function addStylesheet()
      {
          var linkElement = document.createElement("link");
          linkElement.rel = "stylesheet";
          linkElement.href = "resources/selector-line-sourcemap-header-deprecated.php";
          document.head.appendChild(linkElement);
      }
  `);

  Common.Settings.settingForTest('css-source-maps-enabled').set(true);
  TestRunner.addSniffer(BindingsModule.CSSWorkspaceBinding.CSSWorkspaceBinding.prototype, 'updateLocations', step1);
  TestRunner.evaluateInPage('addStylesheet()');

  function step1() {
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step2);
  }

  async function step2() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
