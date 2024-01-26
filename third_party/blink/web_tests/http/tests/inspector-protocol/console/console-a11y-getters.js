(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`<button>Test</button>`,
    'Tests a11y getters exposed via command line API.');

  const evaluate = (expression) => dp.Runtime.evaluate({expression, includeCommandLineAPI: true});

  testRunner.log(await evaluate(`getAccessibleName(document.querySelector('button'))`));
  testRunner.log(await evaluate(`getAccessibleRole(document.querySelector('button'))`));
  testRunner.log(await evaluate(`getAccessibleRole(null)`));
  testRunner.log(await evaluate(`getAccessibleRole(document)`));

  testRunner.completeTest();
});
