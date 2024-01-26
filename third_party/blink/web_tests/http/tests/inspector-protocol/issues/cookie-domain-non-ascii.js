// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
     `Verifies that Set-Cookie lines with a domain attribute including ` +
     `non-ASCII characters file an issue.\n`);

  const origin = 'https://cookie.test:8443';
  const firstPartyUrl = origin +
    '/inspector-protocol/network/resources/hello-world.html';
  await session.navigate(firstPartyUrl);

  await dp.Audits.enable();
  const setCookieUrl = origin +
    '/inspector-protocol/network/resources/set-cookie-non-ascii-domain.php';
  const issuePromise = dp.Audits.onceIssueAdded();
  await session.evaluate(`
    fetch('${setCookieUrl}', {
      method: 'POST',
      credentials: 'include'
    })
  `);
  const issue = await issuePromise;
  testRunner.log(issue.params.issue, 'Issue reported: ', ['requestId']);
  testRunner.completeTest();
})
