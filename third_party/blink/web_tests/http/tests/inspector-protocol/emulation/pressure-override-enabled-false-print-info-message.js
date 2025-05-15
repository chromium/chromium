(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setPressureSourceOverridesEnabled(false) prints info message.');

  await dp.Log.enable();

  testRunner.expectedSuccess(
      'Enabling pressure source override',
      await dp.Emulation.setPressureSourceOverrideEnabled({
        source: 'cpu',
        enabled: true,
      }));

  testRunner.expectedSuccess(
      'Set pressure state override',
      await dp.Emulation.setPressureDataOverride({
        source: 'cpu',
        state: 'serious',
        ownContributionEstimate: 0.2,
      }));

  // Ensure that the system focus and focused frame checks in
  // PressureServiceBase::HasImplicitFocus() pass.
  await dp.Target.activateTarget({targetId: page.targetId()});
  await dp.Emulation.setFocusEmulationEnabled({enabled: true});

  testRunner.log(await session.evaluateAsync(`
      new Promise(async resolve => {
        const observer = new PressureObserver(records => {
          const flattenedRecords = [];
          records.forEach(record => {
            flattenedRecords.push([record.source, record.state, record.ownContributionEstimate]);
          });
          resolve(flattenedRecords);
        });
        await observer.observe('cpu');
      })
    `));

  testRunner.expectedSuccess(
      'Removing pressure source override',
      await dp.Emulation.setPressureSourceOverrideEnabled({
        source: 'cpu',
        enabled: false,
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
