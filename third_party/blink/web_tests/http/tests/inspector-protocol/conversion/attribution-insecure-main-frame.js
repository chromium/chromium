// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that loading a conversion pixel in an insecure context triggers an issue.`);

  await dp.Audits.enable();
  await page.navigate('http://devtools.test:8000/inspector-protocol/resources/empty.html');

  const issuePromise = dp.Audits.onceIssueAdded();
  await page.loadHTML(`<!DOCTYPE html><img src="https://devtools.test:8443/inspector-protocol/conversion/resources/conversion-redirect.php?trigger-data=2"></img>`);
  const issue = await issuePromise;
  testRunner.log(issue.params.issue, "Issue reported: ", ['frame', 'requestId']);
  testRunner.completeTest();
})
