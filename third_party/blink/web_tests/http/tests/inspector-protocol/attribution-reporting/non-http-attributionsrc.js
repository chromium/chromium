// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that using a non-HTTP-family URL as an attributionsrc triggers an issue.`);

  await dp.Audits.enable();

  const issuePromise = dp.Audits.onceIssueAdded();
  await page.loadHTML(`<img attributionsrc="wss://example.com/">`);
  const issue = await issuePromise;
  testRunner.log(issue.params.issue, 'Issue reported: ', ['violatingNodeId']);
  testRunner.completeTest();
})
