(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startURL(
        'https://devtools.test:8443/inspector-protocol/resources/content-security-policy-issue-trusted-types-policy.php?rpt',
        'Breakpoint on Trusted Type violations');
  await dp.Debugger.enable();
  await dp.DOM.enable();
  await dp.DOMDebugger.setBreakOnCSPViolation({violationTypes:['trustedtype-sink-violation','trustedtype-policy-violation']});

  const code = `
      trustedTypes.createPolicy("policy2", {});
      document.body.innerHTML="a";
      `;
  session.evaluate(code);

  const pause1 = await dp.Debugger.oncePaused();
  testRunner.log(`violation type: ${pause1.params.data.violationType}`);
  testRunner.log(`paused reason: ${pause1.params.reason}`);
  dp.Debugger.resume();

  const pause2 = await dp.Debugger.oncePaused();
  testRunner.log(`violation type: ${pause2.params.data.violationType}`);
  testRunner.log(`paused reason: ${pause2.params.reason}`);
  dp.Debugger.resume();

  testRunner.completeTest();
})
