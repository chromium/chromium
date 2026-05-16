(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startBlank(`Tests that deprecation issues are not reported on same-origin iframes but are reports on sandboxed iframes`);
  await dp.Network.enable();
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded(issue => issue.params.issue.code == 'DeprecationIssue');
  page.navigate('https://devtools.test:8443/inspector-protocol/issues/resources/svg-iframe.html');
  page.navigate('https://devtools.test:8443/inspector-protocol/issues/resources/svg-sandbox-iframe.html');
  const result = await promise;
  const issue = result.params.issue.details.deprecationIssueDetails;
  testRunner.log(`Inspector issue type ${issue.type} on url ${issue.sourceCodeLocation.url}`)
  testRunner.completeTest();
})
