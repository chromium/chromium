(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startURL('resources/page-with-fenced-frame.php',
    'Tests that issues for fenced frames are reported to the fenced frame target');
  await dp.Page.enable();

  let attachedToTargetPromise = dp.Target.onceAttachedToTarget();
  await dp.Target.setAutoAttach({ autoAttach: true, waitForDebuggerOnStart: false, flatten: true });
  let { sessionId } = (await attachedToTargetPromise).params;
  let childSession = session.createChild(sessionId);
  let ffdp = childSession.protocol;

  // Wait for FF to finish loading.
  await ffdp.Page.enable();
  ffdp.Page.setLifecycleEventsEnabled({ enabled: true });
  await ffdp.Page.onceLifecycleEvent(event => event.params.name === 'load');

  // Create an issue.
  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
    + encodeURIComponent('name=value; SameSite=None');
  await childSession.evaluateAsync(`fetch('${setCookieUrl}', {method: 'POST', credentials: 'include'})`);

  ffdp.Audits.enable();
  const issue = (await ffdp.Audits.onceIssueAdded()).params.issue;
  testRunner.log(`Issue logged: ${issue.code}`);
  testRunner.completeTest();
});
