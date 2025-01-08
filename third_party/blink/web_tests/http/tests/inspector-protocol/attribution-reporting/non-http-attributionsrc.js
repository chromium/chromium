// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Test that using a non-HTTP-family URL as an attributionsrc triggers an issue.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  await session.evaluate(`
    document.body.innerHTML = '<img attributionsrc="wss://devtools.test:8443/">'
  `);

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['violatingNodeId']);
  testRunner.completeTest();
})
