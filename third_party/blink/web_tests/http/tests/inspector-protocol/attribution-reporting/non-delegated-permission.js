// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {dp} = await testRunner.startURL(
      'https://devtools.test:8443/inspector-protocol/empty.html',
      'Test that an iframe without the attribution-reporting permission gets a devtools warning');

  await dp.Audits.enable();
  const issue = dp.Audits.onceIssueAdded();

  await dp.Runtime.evaluate({expression: `
    document.body.innerHTML = '<iframe src="https://a.devtools.test:8443/inspector-protocol/attribution-reporting/resources/iframe-register-source.html">';
  `});

  testRunner.log((await issue).params.issue, 'Issue reported: ', ['violatingNodeId']);
  testRunner.completeTest();
})
