(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests pressure overrides work in dedicated workers');

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

  await session.evaluate(`var w = new Worker('${
      testRunner.url('resources/pressure-observer-worker.js')}')`);

  testRunner.log('worker update == ' + (await session.evaluateAsync(`
      w.postMessage('ping!');
      new Promise(resolve => w.onmessage = e => resolve(e.data.records))`)));

  testRunner.completeTest();
});
