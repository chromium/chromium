// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Verify that inline style sourceMappingURL is resolved properly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <body id="inspect">
      <pre class="stylesheet-text">.red,body{color:red}body{background-color:red}
      /*# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJzb3VyY2VzIjpbIm1peGluLmxlc3MiLCJ0ZXN0Lmxlc3MiXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUEsS0NJQSxLREhFLE1BQUEsSUNHRixLQUVFLGlCQUFBIn0=*/</pre>
    </body>
    `);
  await TestRunner.evaluateInPagePromise(`
      function embedInlineStyleSheet()
      {
          var style = document.createElement("style");
          style.type = "text/css";
          style.textContent = document.querySelector(".stylesheet-text").textContent;
          document.head.appendChild(style);
      }
  `);

  SDK.TargetManager.TargetManager.instance().addModelListener(SDK.CSSModel.CSSModel, SDK.CSSModel.Events.StyleSheetAdded, function() {});
  TestRunner.evaluateInPage('embedInlineStyleSheet()', onEvaluated);

  function onEvaluated() {
    ElementsTestRunner.selectNodeAndWaitForStyles('inspect', onSelected);
  }

  async function onSelected() {
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, false);
    TestRunner.completeTest();
  }
})();
