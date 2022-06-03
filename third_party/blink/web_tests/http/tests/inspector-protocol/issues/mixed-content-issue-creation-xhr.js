(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Verifies that mixed content issue is created from mixed content XMLHttpRequest.\n`);

  await dp.Network.enable();
  await dp.Audits.enable();
  const issuePromise = dp.Audits.onceIssueAdded();
  let requestId;
  let frameId;
  dp.Network.onRequestWillBeSent(event => {
    if (event.params.request.url === 'http://devtools.test:8000/inspector-protocol/resources/request-data.js') {
      frameId = event.params.frameId;
      requestId = event.params.requestId;
    }
  });

  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/mixed-content-xhr.html');
  const issue = await issuePromise;

  if (requestId !== issue?.params?.issue?.details?.mixedContentIssueDetails?.request?.requestId) {
    testRunner.log('FAIL: requestIds do not match');
  } else {
    testRunner.log('PASS: requestIds match');
  }
  if (frameId !== issue?.params?.issue?.details?.mixedContentIssueDetails?.frame?.frameId) {
    testRunner.log('FAIL: frameIds do not match');
  } else {
    testRunner.log('PASS: frameIds match');
  }

  testRunner.log(issue.params, "Inspector issue: ", ["frameId", "requestId"]);
  testRunner.completeTest();
})