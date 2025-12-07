(async function (testRunner) {
    const { page, session, dp } = await testRunner.startBlank(
        'Tests that Emulation.setLocaleOverride works when sent to a dedicated worker target.');

    await dp.Target.setAutoAttach(
        { autoAttach: true, waitForDebuggerOnStart: false, flatten: true });

    const attachedPromise = dp.Target.onceAttachedToTarget(
        e => e.params.targetInfo.type === 'worker');
    session.evaluate(`new Worker('/inspector-protocol/resources/empty.js');`);
    const attachedToTarget = await attachedPromise;
    const workerDp = session.createChild(attachedToTarget.params.sessionId).protocol;

    const { result: { result: { value: initialLocale } } } = await workerDp.Runtime.evaluate({
        expression: 'Intl.DateTimeFormat().resolvedOptions().locale'
    });

    testRunner.log('\nSet locale to zh_CN in dedicated worker');
    await workerDp.Emulation.setLocaleOverride({ locale: 'zh_CN' });
    await printLocaleSpecificData();

    testRunner.log('\nTried to set override from main page');
    const override = await dp.Emulation.setLocaleOverride({ locale: 'pt-BR' });
    testRunner.log(override.error.message);

    testRunner.log('\nOverwrite locale with fr-FR in dedicated worker');
    await workerDp.Emulation.setLocaleOverride({ locale: 'fr-FR' });
    await printLocaleSpecificData();

    testRunner.log('\nReset locale to default in dedicated worker');
    await workerDp.Emulation.setLocaleOverride();
    const { result: { result: { value: finalLocale } } } = await workerDp.Runtime.evaluate({
        expression: 'Intl.DateTimeFormat().resolvedOptions().locale'
    });
    testRunner.log(`Locale correctly reset to initial value: ${initialLocale === finalLocale}`)

    testRunner.log('\nSet locale to ru-RU in main page after dedicated worker reset');
    await dp.Emulation.setLocaleOverride({ locale: 'ru-RU' });
    await printLocaleSpecificData();

    testRunner.completeTest();

    async function printLocaleSpecificData() {
        const mainPageLocale = await session.evaluate(() => Intl.DateTimeFormat().resolvedOptions().locale);
        testRunner.log(`Detected main page locale: ${mainPageLocale}`);

        const workerLocaleResponse = await workerDp.Runtime.evaluate({
            expression: 'Intl.DateTimeFormat().resolvedOptions().locale'
        });
        const workerLocale = workerLocaleResponse.result.result.value;
        testRunner.log(`Detected dedicated worker locale: ${workerLocale}`);
    }
})