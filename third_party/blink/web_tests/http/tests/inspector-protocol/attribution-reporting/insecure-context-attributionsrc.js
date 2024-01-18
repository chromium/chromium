// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL(
      'http://devtools.test:8000/inspector-protocol/resources/empty.html',
      'Test that clicking an anchor with attributionsrc in an insecure context triggers an issue.');

  await dp.Audits.enable();

  await dp.Runtime.evaluate({expression: `
    document.body.innerHTML = '<a id="adlink" href="https://a.com" attributionsrc="https://b.com" target="_blank">Link</a>';
  `});

  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({
    expression: `document.getElementById('adlink').click()`,
    userGesture: true,
  });

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['violatingNodeId']);
  testRunner.completeTest();
})
