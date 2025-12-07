(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setPressureSourceOverridesEnabled(true) prints info message.');

  await dp.Log.enable();
  // Ensure that the system focus and focused frame checks in
  // PressureServiceBase::HasImplicitFocus() pass.
  await dp.Target.activateTarget({targetId: page.targetId()});
  await dp.Emulation.setFocusEmulationEnabled({enabled: true});

  await session.evaluateAsync(`
        const observer = new PressureObserver(() => {});
        observer.observe('cpu');
    `);

  testRunner.expectedSuccess(
      'Enabling pressure source override',
      await dp.Emulation.setPressureSourceOverrideEnabled({
        source: 'cpu',
        enabled: true,
      }));

  dp.Log.onEntryAdded(event => {
    const entry = event.params.entry;
    // Remove the error code, as it is platform-specific and can change.
    testRunner.log('Log.onEntryAdded');
    testRunner.log(`level: ${entry.level}`);
    testRunner.log(`text: ${entry.text}`);
    testRunner.completeTest();
  });
});
