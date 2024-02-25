(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Regression test for https://crbug.com/1224133.`);

  await dp.Runtime.enable();
  const exceptionThrown = dp.Runtime.onceExceptionThrown();

  const url = `https://127.0.0.1:8443/inspector-protocol/network/resources`;
  session.evaluate(`fetch("${url}/cors-redirect.php");`);
  session.navigate('../resources/exception-simple.html');

  await exceptionThrown;

  testRunner.completeTest();
})
