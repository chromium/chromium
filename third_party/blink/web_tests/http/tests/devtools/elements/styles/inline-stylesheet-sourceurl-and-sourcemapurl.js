// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

(async function() {
  TestRunner.addResult(
      `Verify that in case of complex scenario with both sourceURL and sourceMappingURL in inline stylesheet the sourceMap is resolved properly.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.evaluateInPagePromise(`
      function addStyleSheet() {
          var styleTag = document.createElement('style');
          styleTag.textContent = \`div {color: red}
/*# sourceURL=style.css */
/*# sourceMappingURL=/devtools/elements/styles/resources/selector-line.css.map */\`;
          document.head.appendChild(styleTag);
      }
  `);

  TestRunner.markStep('Adding style sheet');

  await Promise.all([
    TestRunner.evaluateInPagePromise('addStyleSheet()'),
    BindingsTestRunner.waitForSourceMap('selector-line.css.map'),
  ]);
  TestRunner.addResult('SourceMap successfully loaded.');
  TestRunner.completeTest();
})();
