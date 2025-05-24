(async testRunner => {
    const {page, session, dp} = await testRunner.startBlank(
        `Tests that deprecation issues for ISO-2022-JP are reported`);
    await dp.Audits.enable();
    const promise = dp.Audits.onceIssueAdded();

    page.navigate(
        'https://example.test:8443/inspector-protocol/resources/iso_2022_jp.html');

    const result = await promise;
    testRunner.log(result.params, "Inspector issue: ");
    testRunner.completeTest();
  });