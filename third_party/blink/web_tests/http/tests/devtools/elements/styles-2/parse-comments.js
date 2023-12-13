// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that comments in stylesheets are parsed correctly by the DevTools.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      /* color: red */

      @media /* color: red */ not /* color: red */print /* color: red */ {
          /* color: red */
          /* color: red */#main/* color: red */{/* color: red */ background /* color: red */ :/* color: red */ blue /* color: red */;/* color: red */ }
          /* color: red */
      }

      /* color: red */

      @font-face {
        /* color: red */
        /* color: red */font-family/* color: red */:/* color: red */ Example/* color: red */;/* color: red */
        /* color: red */
        /* color: red */src/* color: red */:/* color: red */ url(bogus-example-url)/* color: red */;/* color: red */
        /* color: red */
      }

      /* color: red */

      #main /* color: red */{
        /* color: red */color/* color: red */:/* color: red */ green /* color: red */;/* color: red */
      }
      /* color: red */
      @page /* color: red */:right /* color: red */{/* color: red */
        /* color: red */margin-left/* color: red */:/* color: red */ 3cm/* color: red */;/* color: red */
        /* color: red */margin-right /* color: red */: /* color: red */4cm /* color: red */
      }/*color: red*/

      /* edge cases */
      @import/**/;
      /**/{}
      </style>
      <div id="main"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('main', step1);

  async function step1() {
    TestRunner.addResult('Main style:');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
