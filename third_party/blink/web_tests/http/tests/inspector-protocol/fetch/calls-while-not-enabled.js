(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that calls to methods of Fetch domain return proper error if the domain has not been enabled`);

  const methods = [
    "fulfillRequest",
    "failRequest",
    "continueRequest",
    "continueWithAuth",
    "getResponseBody",
    "takeResponseBodyAsStream",
  ];
  const params = {
    requestId: "does not matter",
    responseCode: 404,
    errorReason: "not found",
    authChallengeResponse: {response: 'kensentme'}
  };
  for (const methodName of methods) {
    const method = dp.Fetch[methodName];
    const response = await method.call(dp.Fetch, params);
    if (!response.error)
      testRunner.log(`${methodName}: FAIL: not an error response`);
    else
      testRunner.log(`${methodName}: code: ${response.error.code} message: ${response.error.message}`);
  }

  testRunner.completeTest();
})
