(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that calls to methods of DeviceAccess domain return proper error if the domain has not been enabled`);

  const methods = ['cancelPrompt', 'selectPrompt'];
  const params = {id: '', deviceId: ''};
  for (const methodName of methods) {
    const method = dp.DeviceAccess[methodName];
    const response = await method.call(dp.DeviceAccess, params);
    if (!response.error) {
      testRunner.log(`${methodName}: FAIL: not an error response`);
    } else {
      testRunner.log(`${methodName}: code: ${response.error.code} message: ${
          response.error.message}`);
    }
  }

  testRunner.completeTest();
});
