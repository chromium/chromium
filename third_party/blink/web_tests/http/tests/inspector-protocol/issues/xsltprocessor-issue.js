(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(
  'Verifies that XSLT Processing Instruction deprecation issue is created');
  await dp.Audits.enable();
  const promise = new Promise(resolve => dp.Audits.onIssueAdded(event => {
    if (event.params.issue.code === "DeprecationIssue") {
      // For some reason, the line number field of the deprecation report is
      // flaky (between line 29 and line 30). So just verify that this is the
      // deprecation issue for XSLT here.
      const source = event.params.issue.details.deprecationIssueDetails.sourceCodeLocation.url;
      resolve({result: source.includes("xslt"), source});
    }
  }));
  await session.navigate('../resources/xsltprocessor.html');
  const result = await promise;
  testRunner.log(result, "Got XSLT deprecation issue: ");
  testRunner.completeTest();
})
