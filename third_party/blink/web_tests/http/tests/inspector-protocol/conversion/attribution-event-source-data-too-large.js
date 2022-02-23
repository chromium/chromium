// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that an attribution redirect with too large attribution event-source data triggers an issue`);

  await dp.Audits.enable();
  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/empty.html');

  const issuePromise = dp.Audits.onceIssueAdded();
  await page.loadHTML(`
    <!DOCTYPE html>
    <img src="https://devtools.test:8443/inspector-protocol/conversion/resources/conversion-redirect.php?trigger-data=0&event-source-trigger-data=42"></img>`);
  const issues = await issuePromise;
  testRunner.log(issues.params.issue, "Issue reported: ", ['requestId']);

  testRunner.completeTest();
})
