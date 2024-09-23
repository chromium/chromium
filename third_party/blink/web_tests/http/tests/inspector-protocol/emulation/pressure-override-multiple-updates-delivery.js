(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests setPressureStateOverride delivers 2 updates with 1 state change');

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

  await session.evaluate(`
    let states = [];

    const firstUpdate = Promise.withResolvers();
    const secondUpdate = Promise.withResolvers();

    const observer = new PressureObserver((records) => {
      for (record of records) {
        states.push([record.source, record.state]);
      }
      if (states.length == 1) {
        firstUpdate.resolve(states);
      } else if (states.length == 2) {
        observer.disconnect();
        secondUpdate.resolve(states);
      }
    });
    observer.observe("cpu");
  `);

  testRunner.expectedSuccess(
      'Received first update.',
      await session.evaluateAsync('firstUpdate.promise'));

  testRunner.expectedSuccess(
      'Set pressure state override to \'serious\'',
      await dp.Emulation.setPressureStateOverride({
        source: 'cpu',
        state: 'serious',
      }));

  testRunner.expectedSuccess(
      'Received second update.',
      await session.evaluateAsync('firstUpdate.promise'));

  testRunner.log(await session.evaluateAsync('secondUpdate.promise'));

  testRunner.completeTest();
});
