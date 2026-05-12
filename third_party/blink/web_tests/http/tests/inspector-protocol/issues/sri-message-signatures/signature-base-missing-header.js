// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank("Verifies issue creation for a missing header in the signature base.");

  await dp.Network.enable();
  await dp.Audits.enable();
  const url = 'inspector-protocol/network/resources/hello-world.html';
  await page.navigate(url);
  const issuePromise = dp.Audits.onceIssueAdded(e => {
    return e.params.issue.code === 'SRIMessageSignatureIssue';
  });

  const signature_input = `signature=("unencoded-digest";sf "x-test-header");keyid="JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";tag="sri"`;
  const signature = `signature=:SbCdPUyjc0IBJjFbVRWs81ucEUcFz87b37nQ63d6kDW+/JvDmET6O5cSdwlddePvlwemLdaWFuY6pQGO+hrkAg==:`;
  const digest = `sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:`;

  let testURL = new URL('/inspector-protocol/resources/sri-message-signature-test.php', self.origin);
  testURL.searchParams.set('input', signature_input);
  testURL.searchParams.set('signature', signature);
  testURL.searchParams.set('digest', digest);
  await session.evaluate(`fetch('${testURL.href}')`);

  // Dump the issue:
  const issue = await issuePromise;
  testRunner.log(issue.params, "Issue reported: ");
  testRunner.completeTest();
})
