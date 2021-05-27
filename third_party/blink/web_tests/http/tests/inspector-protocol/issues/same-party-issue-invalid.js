// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Verifies that Set-Cookie lines with invalid SameParty attribute usage file an issue.\n`);

  const firstPartyUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/hello-world.html';
  await session.navigate(firstPartyUrl);

  await dp.Audits.enable();
  await fetchWithSetCookieResponse('name=value; SameSite=Strict; SameParty; Secure');
  await fetchWithSetCookieResponse('name=value; SameParty');

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
