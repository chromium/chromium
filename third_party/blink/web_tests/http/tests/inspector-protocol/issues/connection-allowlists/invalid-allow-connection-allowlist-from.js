// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank("Verifies issue creation for an unparsable `Allow-Connection-Allowlist-From` response header.");

  await dp.Network.enable();
  await dp.Audits.enable();

  const base = 'https://devtools.test:8443/inspector-protocol/resources/connection-allowlist-test.php';

  // `Allow-Connection-Allowlist-From` is neither `*` nor a serialized origin, so
  // it cannot opt the framed document into its embedder's requirement. Only the
  // first issue is logged; the navigation goes on to be blocked, which reports a
  // separate `EmbeddingRequirementNotSatisfied` issue.
  page.navigate(`${base}?child_allowlist=${encodeURIComponent('(response-origin)')}` +
                `&child=${encodeURIComponent(`${base}?allow_from=invalid`)}`);
  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params, "Issue reported: ");
  testRunner.completeTest();
})
