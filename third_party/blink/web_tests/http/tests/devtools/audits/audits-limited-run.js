// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that audits panel works when only the pwa category is selected.\n');

  await TestRunner.loadModule('audits_test_runner');
  await TestRunner.showPanel('audits');

  const containerElement = AuditsTestRunner.getContainerElement();
  const checkboxes = containerElement.querySelectorAll('.checkbox');
  for (const checkbox of checkboxes) {
    if (checkbox.textElement.textContent === 'Progressive Web App' ||
        checkbox.textElement.textContent === 'Clear storage')
      continue;

    checkbox.checkboxElement.click();
  }

  AuditsTestRunner.dumpStartAuditState();
  AuditsTestRunner.getRunButton().click();

  const {lhr} = await AuditsTestRunner.waitForResults();
  TestRunner.addResult(`\n=============== Audits run ===============`);
  TestRunner.addResult(Object.keys(lhr.audits).sort().join('\n'));

  TestRunner.completeTest();
})();
