// META: flags=--enable-features=AIPromptAPI

(async function(testRunner) {
  const description =
      `Tests that LanguageModel legacy features do not cause deprecation issues in web context.`;
  const {session, dp} = await testRunner.startBlank(description);
  await dp.Audits.enable();
  await dp.Runtime.enable();

  const script = `
    async function test() {
      const results = {};
      // Check interface visibility
      results.languageModelExists = 'LanguageModel' in window;
      if (!results.languageModelExists) {
        return results;
      }

      results.paramsExists = 'params' in LanguageModel;
      results.languageModelParamsExists = 'LanguageModelParams' in window;

      // Test create() WITHOUT legacy options
      try {
        const lmSession1 = await LanguageModel.create(); // No options
        results.createWithoutOptionsFailed = false;
        results.session1TopKExists = 'topK' in lmSession1;
        results.session1TemperatureExists = 'temperature' in lmSession1;
        results.session1OnQuotaOverflowExists = 'onquotaoverflow' in lmSession1
        results.session1OnContextOverflowExists = 'oncontextoverflow' in lmSession1
      } catch (e) {
        results.createWithoutOptionsFailed = true;
        results.createWithoutOptionsError = e.message;
      }

      // Test create() WITH legacy options
      try {
        const lmSession2 = await LanguageModel.create({topK: 5, temperature: 0.5});
        results.createWithOptionsFailed = false;
        results.session2TopKExists = 'topK' in lmSession2;
        results.session2TemperatureExists = 'temperature' in lmSession2;
        results.session2OnQuotaOverflowExists = 'onquotaoverflow' in lmSession2
        results.session2OnContextOverflowExists = 'oncontextoverflow' in lmSession2
      } catch (e) {
        results.createWithOptionsFailed = true;
        results.createWithOptionsError = e.message;
      }
      return results;
    }
    test();
  `;

  let issueAddedPromise = new Promise((resolve) => {
    dp.Audits.onIssueAdded(event => {
      const code = event.params.issue.code;
      if (code === 'DeprecationIssue') {
        const deprecationName =
            event.params.issue.details.deprecationIssueDetails.type;
        if (deprecationName.includes('LanguageModel')) {
          // Log the unexpected issue and fail the test
          testRunner.log(event.params, 'UNEXPECTED Inspector issue: ');
          testRunner.fail(
              'Unexpected LanguageModel deprecation issue in web context: ' +
              deprecationName);
          resolve(false);  // Indicate failure
        }
      }
    });
    // Set a timeout to resolve if no issue is added
    setTimeout(() => {
      resolve(true);  // Indicate success (no issue found)
    }, 2000);
  });

  // Run the script with user gesture since LanguageModel.create() requires it.
  const executionResults = await session.evaluateAsyncWithUserGesture(script);
  testRunner.log({results: executionResults}, 'Test Results:');

  const noDeprecationIssue = await issueAddedPromise;
  if (noDeprecationIssue) {
    testRunner.log('No unexpected LanguageModel deprecation issues found.');
  }
  testRunner.completeTest();
})
