(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Verifies that mixed content issue is created from mixed content js within an iframe\n`);

  await dp.Network.enable();
  await dp.Audits.enable();

  let requestId;
  let frameId;
  let issues = [];
  let count = 0;
  let eventReceived = null;
  let allEventsReceived = new Promise(resolve => eventReceived = () => {if (++count == 3) resolve();
  });

  dp.Network.onRequestWillBeSent(event => {
    if (event.params.request.url === 'http://example.test:8000/inspector-protocol/resources/blank.js') {
      frameId = event.params.frameId;
      requestId = event.params.requestId;
      eventReceived();
    }
  });

  // We expect to receive two issues, one for a speculative prefetch and another for the actual fetch.
  dp.Audits.onIssueAdded(issue => {
    issues.push(issue.params);
    eventReceived();
  });

  await page.navigate('https://devtools.test:8443/inspector-protocol/resources/mixed-content-within-iframe.html');
  await allEventsReceived;

  // No sorting necessary, both issues are identical except requestId
  issues.forEach(issue => testRunner.log(issue, "Inspector issue: ", ["frameId", "requestId"]));

  const match = issues.find(issue =>
    issue.issue?.details?.mixedContentIssueDetails?.request?.requestId === requestId
    && issue.issue?.details?.mixedContentIssueDetails?.frame?.frameId === frameId
  );
  testRunner.log(`requestIds and frameIds match: ${!!match}`)
  testRunner.completeTest();
})
