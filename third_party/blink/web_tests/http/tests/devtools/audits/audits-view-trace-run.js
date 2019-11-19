// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that audits panel renders View Trace button.\n');

  await TestRunner.loadModule('audits_test_runner');
  await TestRunner.showPanel('audits');

  const containerElement = AuditsTestRunner.getContainerElement();
  const checkboxes = containerElement.querySelectorAll('.checkbox');
  for (const checkbox of checkboxes) {
    if (checkbox.textElement.textContent === 'Performance' ||
        checkbox.textElement.textContent === 'Clear storage')
      continue;

    checkbox.checkboxElement.click();
  }

  AuditsTestRunner.dumpStartAuditState();
  AuditsTestRunner.getRunButton().click();

  const {lhr} = await AuditsTestRunner.waitForResults();
  TestRunner.addResult(`\n=============== Audits run ===============`);
  TestRunner.addResult(Object.keys(lhr.audits).sort().join('\n'));

  const waitForShowView = new Promise(resolve => {
    TestRunner.addSniffer(UI.ViewManager.prototype, 'showView', resolve);
  });
  const viewTraceButton = AuditsTestRunner.getResultsElement().querySelector('.view-trace');
  viewTraceButton.click();
  const viewShown = await waitForShowView;
  TestRunner.addResult(`\nShowing view: ${viewShown}`);

  TestRunner.completeTest();
})();
