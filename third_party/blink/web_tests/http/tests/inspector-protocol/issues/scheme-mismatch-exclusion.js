// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async testRunner => {
  // This test requires kEnableSchemeBoundCookies to be enabled in order to pass.
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that Cookie raises issue when schemes are mismatch\n`);

  await dp.Network.enable();
  await dp.Audits.enable();

  // Set the cookie.
  const response = await dp.Network.setCookie({
    url: 'http://example.test:8443',
    secure: false,
    name: 'foo',
    value: 'bar',
  });

  if (response.error)
    testRunner.log(`setCookie failed: ${response.error.message}`);

  // Push events to arrays to prevent async races from causing flakes.
  const requestWillBeSentExtraInfos = [];
  let issueAdded;

  const expectedRequests =
      new Promise(resolve => dp.Network.onRequestWillBeSentExtraInfo(event => {
        requestWillBeSentExtraInfos.push(event.params);
        if (requestWillBeSentExtraInfos.length === 1) {
          resolve();
        }
      }));

  const expectedIssue = dp.Audits.onceIssueAdded(event => {
    // Safely ignore irrelevant issue...
    return event.params.issue.code !== 'QuirksModeIssue';
  });

  page.navigate(
      'https://example.test:8443/inspector-protocol/network/resources/hello-world.html');

  await expectedRequests;

  issueAdded = await expectedIssue;
  testRunner.log(issueAdded.params);

  testRunner.completeTest();
});
