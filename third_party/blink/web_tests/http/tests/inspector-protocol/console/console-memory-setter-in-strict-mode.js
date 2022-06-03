(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests checks that console.memory property can be set in strict mode (crbug.com/468611).');
  var message = await dp.Runtime.evaluate({expression: '"use strict"\nconsole.memory = {};undefined'});
  testRunner.log(message);
  testRunner.completeTest();
})
