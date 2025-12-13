(async function (testRunner) {
    const { page, session, dp } = await testRunner.startBlank(
        'Tests that Emulation.setTimezoneOverride works when sent to a service worker target.');

    const bp = testRunner.browserP();
    await bp.Target.setAutoAttach(
        { autoAttach: true, waitForDebuggerOnStart: false, flatten: true });

    const attachedPromise = bp.Target.onceAttachedToTarget(
        e => e.params.targetInfo.type === 'service_worker');
    session.evaluate(`navigator.serviceWorker.register('/inspector-protocol/resources/empty.js')`);
    const attachedToTarget = await attachedPromise;
    const workerDp = session.createChild(attachedToTarget.params.sessionId).protocol;

    const { result: { result: { value: initialTimezone } } } = await workerDp.Runtime.evaluate({
        expression: 'Intl.DateTimeFormat().resolvedOptions().timeZone'
    });

    testRunner.log('\nSet timezone to Asia/Shanghai in service worker');
    await workerDp.Emulation.setTimezoneOverride({ timezoneId: 'Asia/Shanghai' });
    await printTimezoneSpecificData();

    testRunner.log('\nOverwrite timezone with Europe/Paris in service worker');
    await workerDp.Emulation.setTimezoneOverride({ timezoneId: 'Europe/Paris' });
    await printTimezoneSpecificData();

    testRunner.log('\nReset timezone to default in service worker');
    await workerDp.Emulation.setTimezoneOverride({ timezoneId: '' });
    const { result: { result: { value: finalTimezone } } } = await workerDp.Runtime.evaluate({
        expression: 'Intl.DateTimeFormat().resolvedOptions().timeZone'
    });
    testRunner.log(`Timezone correctly reset to initial value: ${initialTimezone === finalTimezone}`)

    testRunner.completeTest();

    async function printTimezoneSpecificData() {
        const workerLocaleResponse = await workerDp.Runtime.evaluate({
            expression: 'Intl.DateTimeFormat().resolvedOptions().timeZone'
        });
        const workerLocale = workerLocaleResponse.result.result.value;
        testRunner.log(`Detected service worker timezone: ${workerLocale}`);
    }
})