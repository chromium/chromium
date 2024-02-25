// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`This test checks that style sheets hosted inside shadow roots could be inspected.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="host"></div>
      <template id="tmpl">
          <style> .red { color: red; } </style>
          <div id="inner" class="red">hi!</div>
      </template>
    `);
  await TestRunner.evaluateInPagePromise(`
      function createShadowRoot()
      {
          var template = document.querySelector('#tmpl');
          var root = document.querySelector('#host').attachShadow({mode: 'open'});
          root.appendChild(template.content.cloneNode(true));
      }
  `);

  TestRunner.runTestSuite([
    function testInit(next) {
      TestRunner.evaluateInPage('createShadowRoot()', callback);
      function callback() {
        ElementsTestRunner.selectNodeAndWaitForStyles('inner', next);
      }
    },

    async function testDumpStyles(next) {
      await ElementsTestRunner.dumpSelectedElementStyles(true);
      next();
    }
  ]);
})();
