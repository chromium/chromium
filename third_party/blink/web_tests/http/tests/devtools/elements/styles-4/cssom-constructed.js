// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that constructed stylesheets appear properly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected">Text</div>
    `);
  await TestRunner.evaluateInPagePromise(`
      const s = new CSSStyleSheet();
      s.replaceSync('div {color: red}');
      document.adoptedStyleSheets = [s];
  `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', modify);

  async function modify() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);

    TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetChanged, onStyleSheetChanged, this);

    function onStyleSheetChanged(event) {
      TestRunner.addResult('StyleSheetChanged triggered');
      TestRunner.completeTest();
    }

    await TestRunner.evaluateInPagePromise(`
      s.insertRule('div {color: green}');
    `);
  }
})();
