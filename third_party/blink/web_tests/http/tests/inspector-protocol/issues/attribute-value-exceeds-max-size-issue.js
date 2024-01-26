// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Verifies that Set-Cookie lines with an attribute size that exceeds the max size files an issue.\n`);

  const firstPartyUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  await session.navigate(firstPartyUrl);

  await dp.Audits.enable();
  await fetchWithSetCookieResponse('name=value;path=/' + 'a'.repeat(1024));
  testRunner.completeTest();

  async function fetchWithSetCookieResponse(cookieLine) {
    const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
    + encodeURIComponent(cookieLine);

    const issuePromise = dp.Audits.onceIssueAdded();
    await session.evaluate(`fetch('${setCookieUrl}', {method: 'POST', credentials: 'include'})`);
    const issue = await issuePromise;
    testRunner.log(issue.params.issue, "Issue reported: ", ['requestId']);
  }
})
