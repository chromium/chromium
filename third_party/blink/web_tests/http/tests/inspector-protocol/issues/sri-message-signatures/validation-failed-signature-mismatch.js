// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const valid_digest = "sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:";
const valid_signature = `signature=:SbCdPUyjc0IBJjFbVRWs81ucEUcFz87b37nQ63d6kDW+/JvDmET6O5cSdwlddePvlwemLdaWFuY6pQGO+hrkAg==:`;
const valid_signature_input = `signature=("unencoded-digest";sf);keyid="JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";tag="ed25519-integrity"`;

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank("Verifies issue creation for signature validation failure.");

  // Navigate to an arbitrary file, enable audits, and wait for an issue to
  // appear. We'll perform the test via the `fetch()` call below.
  await dp.Network.enable();
  await dp.Audits.enable();
  const url = 'inspector-protocol/network/resources/hello-world.html';
  await page.navigate(url);
  const issuePromise = dp.Audits.onceIssueAdded(e => {
    return e.params.issue.code === 'SRIMessageSignatureIssue';
  });

  let testURL = new URL('/inspector-protocol/resources/sri-message-signature-test.php', self.origin);
  testURL.searchParams.set('digest', "sha-256=:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=:");
  testURL.searchParams.set('signature', valid_signature);
  testURL.searchParams.set('input', valid_signature_input);
  await session.evaluate(`fetch('${testURL.href}')`);

  // Dump the issue:
  const issue = await issuePromise;
  testRunner.log(issue.params, "Issue reported: ");
  testRunner.completeTest();
})
