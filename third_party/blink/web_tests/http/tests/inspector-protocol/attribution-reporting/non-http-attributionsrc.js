// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that using a non-HTTP-family URL as an attributionsrc triggers an issue.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({expression: `
    document.body.innerHTML = '<img attributionsrc="wss://example.com/">';
  `});

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['violatingNodeId']);
  testRunner.completeTest();
})
