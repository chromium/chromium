(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Report stylesheet loading failures');

  await dp.DOM.enable();
  await dp.Network.enable();
  await dp.CSS.enable();
  await dp.Page.enable();
  await dp.Audits.enable();

  session.navigate('../resources/stylesheet-loading-issues.html');

  const issues = [];
  await dp.Audits.onceIssueAdded(e => {
    const {params: {issue}} = e;
    const {code, details: {stylesheetLoadingIssueDetails}} = issue;
    if (code === 'StylesheetLoadingIssue') {
      issues.push(stylesheetLoadingIssueDetails);
    }
    return issues.length === 6;
  });

  function issueComparator(i) {
    return `${i.styleSheetLoadingIssueReason}:${i.sourceCodeLocation.url}:${
        i.sourceCodeLocation.lineNumber}:${i.sourceCodeLocation.columnNumber}:${
        i.failedRequestInfo?.url}:${i.failedRequestInfo?.failureMessage}:${
        i.failedRequestInfo?.requestId}`;
  }
  issues.sort((a, b) => {
    const cmpA = issueComparator(a), cmpB = issueComparator(b);
    return cmpA < cmpB ? -1 : cmpA > cmpB ? 1 : 0;
  });

  for (const {
         styleSheetLoadingIssueReason,
         sourceCodeLocation: {url, lineNumber, columnNumber},
         failedRequestInfo
       } of issues) {
    testRunner.log(`Reason: ${styleSheetLoadingIssueReason}`);
    testRunner.log(`Location: ${url} @ ${lineNumber + 1}:${columnNumber + 1}`);
    if (failedRequestInfo) {
      const {url, failureMessage, requestId} = failedRequestInfo;
      testRunner.log(`Request: ${failureMessage} ${url} ${Boolean(requestId)}`);
    } else {
      testRunner.log(`Request: ${failedRequestInfo}`);
    }
  }

  testRunner.completeTest();
});
