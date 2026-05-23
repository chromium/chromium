(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setPressureStateOverride delivers updates');

  await dp.Emulation.setPressureSourceOverrideEnabled({
    source: 'cpu',
    enabled: true,
  });

  testRunner.expectedSuccess(
      'Set pressure state override to \'critical\'',
      await dp.Emulation.setPressureStateOverride({
        source: 'cpu',
        state: 'critical',
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
            flattenedRecords.push([record.source, record.state]);
          });
          resolve(flattenedRecords);
        });
        await observer.observe('cpu');
      })
    `));

  testRunner.completeTest();
});
