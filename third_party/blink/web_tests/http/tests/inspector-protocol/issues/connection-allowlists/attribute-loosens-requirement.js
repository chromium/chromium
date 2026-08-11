// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank("Verifies issue creation when a frame's `connectionallowlist` attribute is less strict than the requirement it inherits from an ancestor.");

  await dp.Network.enable();
  await dp.Audits.enable();

  const base = 'https://devtools.test:8443/inspector-protocol/resources/connection-allowlist-test.php';

  // The middle document opts into its embedder's requirement, so that a
  // requirement is already in effect when it frames the innermost document. That
  // innermost frame's attribute additionally lists https://extra.test/, which
  // would loosen the inherited requirement, so it is discarded.
  const innermost = `${base}?allow_from=*`;
  const middle = `${base}?allow_from=*` +
      `&child_allowlist=${encodeURIComponent('("https://extra.test/" response-origin)')}` +
      `&child=${encodeURIComponent(innermost)}`;
  page.navigate(`${base}?child_allowlist=${encodeURIComponent('(response-origin)')}` +
                `&child=${encodeURIComponent(middle)}`);
  const issue = await dp.Audits.onceIssueAdded();

  testRunner.log(issue.params, "Issue reported: ");
  testRunner.completeTest();
})
