// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank("Verifies issue creation when a framed document neither accepts its embedder's Connection-Allowlist requirement nor delivers one that is at least as strict.");

  await dp.Network.enable();
  await dp.Audits.enable();

  const base = 'https://devtools.test:8443/inspector-protocol/resources/connection-allowlist-test.php';

  // The framed document neither sends `Allow-Connection-Allowlist-From` nor
  // delivers a `Connection-Allowlist` of its own, so it cannot be displayed.
  page.navigate(`${base}?child_allowlist=${encodeURIComponent('(response-origin)')}` +
                `&child=${encodeURIComponent(base)}`);
  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params, "Issue reported: ");
  testRunner.completeTest();
})
