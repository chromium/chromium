// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Test that an attributionsrc that redirects to an insecure origin and tries to register a source triggers an issue.');

  await dp.Audits.enable();

  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({expression: `
    document.body.innerHTML = '<img attributionsrc="https://devtools.test:8443/inspector-protocol/attribution-reporting/resources/redirect-to-insecure-origin-and-register-source.php">';
  `});

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['request']);
  testRunner.completeTest();
})
