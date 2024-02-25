// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that shorthand is marked as overloaded if all its longhands are overloaded.\n`);

  await TestRunner.showPanel('elements');

  await TestRunner.loadHTML(`
    <style>
      .foo {
        /* Font longhands */
        font-style: normal;
        font-variant-ligatures: normal;
        font-variant-caps: normal;
        font-variant-numeric: normal;
        font-variant-east-asian: normal;
        font-weight: normal;
        font-stretch: normal;
        font-size: 1.2em;
        line-height: 1;
        font-family: "Arial", sans-serif;

        /* Margin shorthand */
        margin: 10px;
      }

      div {
        /* Margin longhands */
        margin-left: 0px !important;
        margin-right: 0px !important;
        margin-top: 0px !important;
        margin-bottom: 0px !important;
      }

      body {
        /* Font shorthand */
        font: 1.2em "Arial", sans-serif;
      }

    </style>
    <div id=inspected class=foo></div>
  `);

  await ElementsTestRunner.selectNodeAndWaitForStylesPromise('inspected');
  await ElementsTestRunner.dumpSelectedElementStyles(true, false);
  TestRunner.completeTest();
})();
