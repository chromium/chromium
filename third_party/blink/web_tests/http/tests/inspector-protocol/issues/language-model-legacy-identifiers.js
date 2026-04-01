(async function(testRunner) {
  const description =
      `Tests LanguageModel renamed API surfaces and deprecation issues`;
  const {session, dp} = await testRunner.startBlank(description);
  await dp.Audits.enable();
  await dp.Runtime.enable();

  const script = `
    async function test() {
      const results = {};
      const lmSession = await LanguageModel.create();
      results.inputQuotaExists = 'inputQuota' in lmSession;
      results.inputUsageExists = 'inputUsage' in lmSession;
      results.measureInputUsageExists = 'measureInputUsage' in lmSession;
      results.onquotaoverflowExists = 'onquotaoverflow' in lmSession;

      // Trigger deprecations by accessing/calling them if they exist.
      if (results.inputQuotaExists) {
        lmSession.inputQuota;
      }
      if (results.inputUsageExists) {
        lmSession.inputUsage;
      }
      if (results.measureInputUsageExists) {
        await lmSession.measureInputUsage("test");
      }
      if (results.onquotaoverflowExists) {
        lmSession.onquotaoverflow = () => {};
      }

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
