(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that Page.addScriptToEvaluateOnLoad is executed in the order of addition');
  dp.Runtime.enable();
  dp.Page.enable();

  const scriptIds = [];
  dp.Runtime.onConsoleAPICalled(msg => testRunner.log(msg.params.args[0].value));

  testRunner.log('Adding scripts');
  for (let i = 0; i < 5; ++i) {
    const result = await dp.Page.addScriptToEvaluateOnNewDocument({source: `
      console.log('message from ${i}');
    `});
    scriptIds.push(result.result.identifier);
  }

  await session.navigate('../resources/blank.html');

  testRunner.log('Removing scripts');
  for (let identifier of scriptIds) {
    const response = await dp.Page.removeScriptToEvaluateOnNewDocument({identifier});
    if (!response.result)
      testRunner.log('Failed script removal');
  }

  await session.navigate('../resources/blank.html');

  testRunner.completeTest();
})
