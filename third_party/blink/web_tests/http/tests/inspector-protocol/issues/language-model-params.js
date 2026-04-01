(async function(testRunner) {
  const description =
      `Tests LanguageModel params API surfaces and deprecation issues`;
  const {session, dp} = await testRunner.startBlank(description);
  await dp.Audits.enable();
  await dp.Runtime.enable();

  const script = `
    async function test() {
      const results = {};
      results.languageModelParamsExists = 'LanguageModelParams' in window;
      results.paramsExists = 'params' in LanguageModel;

      // Trigger deprecation by calling params() if it exists.
      if (results.paramsExists) {
        await LanguageModel.params();
      }

      // Test create() WITHOUT sampling parameter options.
      const lmSession1 = await LanguageModel.create(); // No options
      results.session1TopKExists = 'topK' in lmSession1;
      results.session1TemperatureExists = 'temperature' in lmSession1;

      // Test create() WITH sampling parameter options.
      const lmSession2 = await LanguageModel.create({topK: 5, temperature: 0.5});
      results.session2TopKExists = 'topK' in lmSession2;
      results.session2TemperatureExists = 'temperature' in lmSession2;

      return results;
    }
    test();
  `;

  let issueAddedPromise = new Promise((resolve) => {
    let issueFound = false;
    dp.Audits.onIssueAdded(event => {
      if (event.params.issue.code === 'DeprecationIssue') {
        const type = event.params.issue.details.deprecationIssueDetails.type;
        if (type.includes('LanguageModel')) {
          testRunner.log('LanguageModel deprecation issue found: ' + type);
          issueFound = true;
        }
      }
    });
    setTimeout(() => {
      if (!issueFound) {
        testRunner.log('No LanguageModel deprecation issues found.');
      }
      resolve();
    }, 2000);
  });

  // Run the script with user gesture since LanguageModel.create() requires it.
  const executionResults = await session.evaluateAsyncWithUserGesture(script);
  testRunner.log({results: executionResults}, 'Test Results:');
  await issueAddedPromise;

  testRunner.completeTest();
})
