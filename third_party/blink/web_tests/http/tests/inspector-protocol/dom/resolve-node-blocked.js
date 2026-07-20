(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that JS object to node resolution still works even if script evals are prohibited by Content-Security-Policy. The test passes if it doesn't crash. Bug 78705.`);

  await page.navigate('../resources/content-security-policy-issue-eval.php');

  await dp.DOM.enable();
  await dp.DOM.getDocument();
  const evaluateResponse = await dp.Runtime.evaluate({expression: 'document'});
  testRunner.log('didReceiveDocumentObject');

  const requestNodeResponse = await dp.DOM.requestNode(
      {objectId: evaluateResponse.result.result.objectId});
  testRunner.log('didRequestNode error = ' +
                 (requestNodeResponse.result.nodeId ? 'null' : 'error'));

  testRunner.completeTest();
})
