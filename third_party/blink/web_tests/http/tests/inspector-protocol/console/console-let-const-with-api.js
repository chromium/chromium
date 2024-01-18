(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests how let/const declarations interact with command line api.');

  var response = await dp.Runtime.evaluate({expression: 'let a = 42;'});
  failIfError(response);
  testRunner.log(`first 'let a = 1;' result: wasThrown = ${!!response.result.exceptionDetails}`);

  response = await dp.Runtime.evaluate({expression: 'let a = 239;'});
  failIfError(response);
  testRunner.log(`second 'let a = 1;' result: wasThrown = ${!!response.result.exceptionDetails}`);
  if (response.result.exceptionDetails)
    testRunner.log('exception message: ' + response.result.exceptionDetails.text + ' ' + response.result.exceptionDetails.exception.description);

  response = await dp.Runtime.evaluate({expression: 'a'});
  failIfError(response);
  testRunner.log(JSON.stringify(response.result));

  var methods = [ '$', '$$', '$x', 'dir', 'dirxml', 'keys', 'values', 'profile', 'profileEnd',
      'monitorEvents', 'unmonitorEvents', 'inspect', 'copy', 'clear', 'getEventListeners',
      'debug', 'undebug', 'monitor', 'unmonitor', 'table' ];
  for (var method of methods) {
    response = await dp.Runtime.evaluate({expression: method, includeCommandLineAPI: true});
    failIfError(response);
    testRunner.log(response.result.result.description);
  }
  testRunner.completeTest();

  function failIfError(response) {
    if (response && response.error) {
      testRunner.log('FAIL: ' + JSON.stringify(response.error));
      testRunner.completeTest();
    }
  }
})
