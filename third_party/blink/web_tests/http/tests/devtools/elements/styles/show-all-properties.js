// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Verifies that large rules are truncated and can be fully expanded.\n`);
  await TestRunner.showPanel('elements');
  var ruleText = '\n';
  for (var i = 0; i < 200; i++)
    ruleText += '--var-' + i + ': ' + i + 'px;\n';
  await TestRunner.loadHTML(`
      <style>
      #inspected {${ruleText}}
      </style>
      <div id="inspected">Text</div>
    `);

  await new Promise(x => ElementsTestRunner.selectNodeAndWaitForStyles('inspected', x));
  TestRunner.addResult('Before showing all properties:')
  await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);

  TestRunner.addResult('After showing all properties:')
  ElementsTestRunner.firstMatchedStyleSection().showAllButton.click();
  await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
  TestRunner.completeTest();
})();
