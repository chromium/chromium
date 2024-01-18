(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const { page, dp } = await testRunner.startBlank(`Test JS Injection Delegate-CH`);
    await dp.Network.enable();
    await dp.Audits.enable();
    const promise = dp.Audits.onceIssueAdded();
    page.navigate('https://devtools.test:8443/inspector-protocol/resources/client-hint-delegate-ch-meta-tag-javascript-injection.html');
    const result = await promise;
    testRunner.log(result.params, "Inspector issue: ");
    testRunner.completeTest();
  })
