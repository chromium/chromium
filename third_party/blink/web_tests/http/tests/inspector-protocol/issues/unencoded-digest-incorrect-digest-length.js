// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank("Verifies issue creation for an `Unencoded-Digest` header with an incorrect digest length.");

  // Navigate to an arbitrary file, enable audits, and wait for an issue to
  // appear. We'll perform the test via the `fetch()` call below.
  await dp.Network.enable();
  await dp.Audits.enable();
  const url = 'inspector-protocol/network/resources/hello-world.html';
  await page.navigate(url);
  const issuePromise = dp.Audits.onceIssueAdded();

  let testURL = new URL('/inspector-protocol/resources/unencoded-digest-test.php', self.origin);
  testURL.searchParams.set('digest', `sha-256=:SGVsbG8sIFdvcmxkIQ==:`);
  await session.evaluate(`fetch('${testURL.href}')`);

  // Dump the issue:
  const issue = await issuePromise;
  testRunner.log(issue.params, "Issue reported: ");
  testRunner.completeTest();
})
