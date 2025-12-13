(async function (testRunner) {
    const { page, session, dp } = await testRunner.startBlank(
        'Tests that Emulation.setLocaleOverride works when sent to a shared worker target.');

    const bp = testRunner.browserP();
    await bp.Target.setAutoAttach(
        { autoAttach: true, waitForDebuggerOnStart: false, flatten: true });

    const attachedPromise = bp.Target.onceAttachedToTarget(
        e => e.params.targetInfo.type === 'shared_worker');
    session.evaluate(`new SharedWorker('/inspector-protocol/resources/empty.js');`);
    const attachedToTarget = await attachedPromise;
    const workerDp = session.createChild(attachedToTarget.params.sessionId).protocol;

    const { result: { result: { value: initialLocale } } } = await workerDp.Runtime.evaluate({
        expression: 'Intl.DateTimeFormat().resolvedOptions().locale'
    });

    testRunner.log('\nSet locale to zh_CN in shared worker');
    await workerDp.Emulation.setLocaleOverride({ locale: 'zh_CN' });
    await printLocaleSpecificData();

    testRunner.log('\nOverwrite locale with fr-FR in shared worker');
    await workerDp.Emulation.setLocaleOverride({ locale: 'fr-FR' });
    await printLocaleSpecificData();

    testRunner.log('\nReset locale to default in shared worker');
    await workerDp.Emulation.setLocaleOverride();
    const { result: { result: { value: finalLocale } } } = await workerDp.Runtime.evaluate({
        expression: 'Intl.DateTimeFormat().resolvedOptions().locale'
    });
    testRunner.log(`Locale correctly reset to initial value: ${initialLocale === finalLocale}`)

    testRunner.completeTest();

    async function printLocaleSpecificData() {
        const workerLocaleResponse = await workerDp.Runtime.evaluate({
            expression: 'Intl.DateTimeFormat().resolvedOptions().locale'
        });
        const workerLocale = workerLocaleResponse.result.result.value;
        testRunner.log(`Detected shared worker locale: ${workerLocale}`);
    }
})