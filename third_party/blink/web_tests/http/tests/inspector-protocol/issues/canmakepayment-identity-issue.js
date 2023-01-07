(async function (testRunner) {
  const {session, dp} = await testRunner.startBlank(
    'Tests that deprecation issues are reported for ' +
    '"canmakepayment" identity fields');

  await dp.Audits.enable();
  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const dir = '/inspector-protocol/resources';
  await session.navigate(`${dir}/canmakepayment-identity.html`);

  const attachedPromise = dp.Target.onceAttachedToTarget();
  session.evaluate(
      `navigator.serviceWorker.register("${dir}/canmakepayment-identity.js");`);
  const attachedToTarget = await attachedPromise;

  const swdp = session.createChild(attachedToTarget.params.sessionId).protocol;

  const issueAddedPromise = swdp.Audits.onceIssueAdded();

  await Promise.all([
    swdp.Audits.enable(),
    swdp.Runtime.runIfWaitingForDebugger(),
  ]);

  const issueAddedResult = await issueAddedPromise;
  testRunner.log(issueAddedResult.params, 'Inspector issue: ');
  testRunner.completeTest();
})
