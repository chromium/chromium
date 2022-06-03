// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that clicking an attribution link in a secure iframe thats embedded in an insecure main frame triggers an issue.`);

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  await page.navigate('http://devtools.test:8000/inspector-protocol/resources/empty.html');

  session.evaluate(`
    const frame = document.createElement('iframe');
    frame.src = 'https://devtools.test:8443/inspector-protocol/conversion/resources/impression.html';
    frame.allow = 'attribution-reporting';
    document.body.appendChild(frame);
  `);

  const {params} = await dp.Target.onceAttachedToTarget();
  const dpFrame = session.createChild(params.sessionId).protocol;

  // Wait for the script tag in 'impression.html' to be parsed.
  dpFrame.Page.enable();
  await dpFrame.Page.onceLoadEventFired();

  await dpFrame.Runtime.evaluate({
    expression: `
    createImpressionAnchor('adlink', 'https://a.com', 42, 'https://does-not-matter.com', '_blank');`,
  });

  await dpFrame.Audits.enable();
  const issuePromise = dpFrame.Audits.onceIssueAdded();
  await dpFrame.Runtime.evaluate({expression: `document.getElementById('adlink').click()`, userGesture: true});
  const issue = await issuePromise;
  testRunner.log(issue.params.issue, "Issue reported: ", ['frame', 'violatingNodeId']);

  const violatingFrameId = issue.params.issue.details.attributionReportingIssueDetails.frame.frameId;
  const reportingFrameId = params.targetInfo.targetId;
  if (violatingFrameId === reportingFrameId) {
    testRunner.log("Success: violating frame and reporting frame match");
  } else {
    testRunner.log("Failure: violating frame and reporting frame do not match, but are expected to be the same");
  }
  testRunner.completeTest();
})
