// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Test that a site with disabled conversion measurement Permissions Policy reports an issue when a conversion pixel is loaded`);

  await dp.Audits.enable();
  await dp.Network.enable();
  await page.navigate('https://devtools.test:8443/inspector-protocol/conversion/resources/permissions-policy-no-conversion-measurement.php');

  const eventPromises = [dp.Network.onceRequestWillBeSent(), dp.Audits.onceIssueAdded()];
  await page.loadHTML(`
    <!DOCTYPE html>
    <img src="https://devtools.test:8443/inspector-protocol/conversion/resources/conversion-redirect.php?trigger-data=2"></img>`);

  const [requestWillBeSent, issue] = await Promise.all(eventPromises);
  testRunner.log(issue.params.issue, "Issue reported: ", ['frame', 'request', 'violatingNodeId']);

  const requestWillBeSentRequestId = requestWillBeSent.params.requestId;
  const issueAddedIdRequestId = issue.params.issue.details.attributionReportingIssueDetails.request.requestId;
  if (requestWillBeSentRequestId === issueAddedIdRequestId) {
    testRunner.log('Success: Request IDs reported from "Network.requestWillBeSent" and "Audits.issueAdded" match');
  } else {
    testRunner.log('Failure: Request IDs reported from "Network.requestWillBeSent" and "Audits.issueAdded" DON\'T match');
    testRunner.log(`${requestWillBeSentRequestIdd} (requestWillBeSent) vs ${issueAddedIdRequestId} (issueAdded)`);
  }
  testRunner.completeTest();
})
