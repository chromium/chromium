(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startBlank(`Tests that data: URLs in SVGUseElement deprecation issues are reported`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate(`document.createElementNS('http://www.w3.org/2000/svg', 'use').setAttribute('href', 'data:image/svg+xml,foo#bar')`);
  const result = await promise;
  testRunner.log(result.params, "Inspector issue: ");
  testRunner.completeTest();
})
