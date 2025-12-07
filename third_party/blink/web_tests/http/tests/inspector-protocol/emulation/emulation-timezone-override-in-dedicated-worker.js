(async function (testRunner) {
    const { page, session, dp } = await testRunner.startBlank(
        'Tests that Emulation.setTimezoneOverride works when sent to a dedicated worker target.');

    await dp.Target.setAutoAttach(
        { autoAttach: true, waitForDebuggerOnStart: false, flatten: true });

    const attachedPromise = dp.Target.onceAttachedToTarget(
        e => e.params.targetInfo.type === 'worker');
    session.evaluate(`new Worker('/inspector-protocol/resources/empty.js');`);
    const attachedToTarget = await attachedPromise;
    const workerDp = session.createChild(attachedToTarget.params.sessionId).protocol;

    const { result: { result: { value: initialTimezone } } } = await workerDp.Runtime.evaluate({
        expression: 'Intl.DateTimeFormat().resolvedOptions().timeZone'
    });

    testRunner.log('\nSet timezone to Asia/Shanghai in dedicated worker');
    await workerDp.Emulation.setTimezoneOverride({ timezoneId: 'Asia/Shanghai' });
    await printTimezoneSpecificData();

    testRunner.log('\nTried to set override from main page');
    const override = await dp.Emulation.setTimezoneOverride({ timezoneId: 'America/Sao_Paulo' });
    testRunner.log(override.error.message);

    testRunner.log('\nOverwrite timezone with Europe/Paris in dedicated worker');
    await workerDp.Emulation.setTimezoneOverride({ timezoneId: 'Europe/Paris' });
    await printTimezoneSpecificData();

    testRunner.log('\nReset timezone to default in dedicated worker');
    await workerDp.Emulation.setTimezoneOverride({ timezoneId: '' });
    const { result: { result: { value: finalTimezone } } } = await workerDp.Runtime.evaluate({
        expression: 'Intl.DateTimeFormat().resolvedOptions().timeZone'
    });
    testRunner.log(`Timezone correctly reset to initial value: ${initialTimezone === finalTimezone}`)

    testRunner.log('\nSet timezone to Europe/Moscow in main page after dedicated worker reset');
    await dp.Emulation.setTimezoneOverride({ timezoneId: 'Europe/Moscow' });
    await printTimezoneSpecificData();

    testRunner.completeTest();

    async function printTimezoneSpecificData() {
        const mainPageTimezone = await session.evaluate(() => Intl.DateTimeFormat().resolvedOptions().timeZone);
        testRunner.log(`Detected main page timezone: ${mainPageTimezone}`);

        const workerTimezoneResponse = await workerDp.Runtime.evaluate({
            expression: 'Intl.DateTimeFormat().resolvedOptions().timeZone'
        });
        const workerTimezone = workerTimezoneResponse.result.result.value;
        testRunner.log(`Detected dedicated worker timezone: ${workerTimezone}`);
    }
})