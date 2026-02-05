// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank("Verifies issue creation for a `Connection-Allowlist` header that is not an inner list.");

  await dp.Network.enable();
  await dp.Audits.enable();

  page.navigate('https://devtools.test:8443/inspector-protocol/resources/connection-allowlist-test.php?enforced="not-a-list"');
  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params, "Issue reported: ");
  testRunner.completeTest();
})
