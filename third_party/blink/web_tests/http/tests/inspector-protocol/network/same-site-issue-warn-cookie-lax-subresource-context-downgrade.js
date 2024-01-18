(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Verifies that a subresource accessing a SameSite=Lax cookie across schemes triggers a context downgrade inspector issue.\n`);

  const protocolMessages = [];
  const originalDispatchMessage = DevToolsAPI.dispatchMessage;
  DevToolsAPI.dispatchMessage = (message) => {
    protocolMessages.push(message);
    originalDispatchMessage(message);
  };
  window.onerror = (msg) => testRunner.log('onerror: ' + msg);
  window.onunhandledrejection = (e) => testRunner.log('onunhandledrejection: ' + e.reason);
  let errorForLog = new Error();
  setTimeout(() => {
    testRunner.log(protocolMessages);
    testRunner.die('Timeout', errorForLog);
  }, 28000);
  await dp.Network.enable();
  await dp.Audits.enable();

  await session.navigate('http://cookie.test:8000/inspector-protocol/resources/empty.html');
  errorForLog = new Error();

  const setCookieUrl = 'https://cookie.test:8443/inspector-protocol/network/resources/set-cookie.php?cookie='
      + encodeURIComponent('name=value; SameSite=Lax');
  session.evaluate(`fetch('${setCookieUrl}', {method: 'POST', credentials: 'include'})`);
  errorForLog = new Error();
  const issue = await dp.Audits.onceIssueAdded();
  errorForLog = new Error();
  testRunner.log(issue.params, 'Inspector issue:');

  testRunner.completeTest();
});
