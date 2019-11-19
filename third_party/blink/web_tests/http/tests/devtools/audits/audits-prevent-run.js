// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  // about:blank never fires a load event so just wait until we see the URL change
  function navigateToAboutBlankAndWait() {
    var listenerPromise = new Promise(resolve => {
      SDK.targetManager.addEventListener(SDK.TargetManager.Events.InspectedURLChanged, resolve);
    });

    TestRunner.navigate('about:blank');
    return listenerPromise;
  }

  TestRunner.addResult('Tests that audits panel prevents run of unauditable pages.\n');

  await TestRunner.loadModule('audits_test_runner');
  await TestRunner.showPanel('audits');

  TestRunner.addResult('\n\n**Prevents audit with no categories**');
  AuditsTestRunner.openStartAudit();
  var containerElement = AuditsTestRunner.getContainerElement();
  var checkboxes = containerElement.querySelectorAll('.checkbox');
  checkboxes.forEach(checkbox => checkbox.checkboxElement.click());
  AuditsTestRunner.dumpStartAuditState();

  TestRunner.addResult('\n\n**Allows audit with a single category**');
  checkboxes[0].checkboxElement.click();
  AuditsTestRunner.dumpStartAuditState();

  TestRunner.addResult('\n\n**Allows audit on undockable page**');
  // Extension page and remote debugging previously caused crashes (crbug.com/734532)
  // However, the crashes have been resolved, so these should now pass.
  AuditsTestRunner.forcePageAuditabilityCheck();
  AuditsTestRunner.dumpStartAuditState();

  TestRunner.addResult('\n\n**Prevents audit on internal page**');
  await navigateToAboutBlankAndWait();
  TestRunner.addResult(`URL: ${TestRunner.mainTarget.inspectedURL()}`);
  AuditsTestRunner.dumpStartAuditState();

  TestRunner.completeTest();
})();
