// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startBlank(
      'Test that an untrustworthy attributionsrc triggers an issue when the img is preloaded.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  await page.navigate(
      'https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/preload.html');

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['violatingNodeId']);
  testRunner.completeTest();
})
