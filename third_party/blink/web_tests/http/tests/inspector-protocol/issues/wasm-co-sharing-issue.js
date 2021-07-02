(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://a.wasm.test:8443/inspector-protocol/resources/empty.html',
      `Verifies that sending a Wasm Module to a same-site context causes an issue.\n`);

  await dp.Audits.enable();
  await dp.Runtime.enable();
  const issuePromise = dp.Audits.onceIssueAdded();

  const iframeCreatedResult = await session.evaluateAsync(`
    window.myframe = document.createElement('iframe');
    window.myframe.src = 'https://b.wasm.test:8443/inspector-protocol/resources/onmessage.html';
    document.body.appendChild(window.myframe);
    const loadPromise = new Promise(r => window.myframe.onload = r);
    loadPromise
  `);
  testRunner.log(`Iframe Created: ${iframeCreatedResult !== undefined}`);

  const postMessageSent = await session.evaluateAsync(`
    document.domain = 'wasm.test';
    const instancePromise = WebAssembly.instantiateStreaming(fetch('https://a.wasm.test:8443/inspector-protocol/resources/add.wasm.php'), {});
    instancePromise.then(pair => {
      window.myframe.contentWindow.postMessage(pair.module, 'https://b.wasm.test:8443');
      return pair.module;
    }).catch(() => "error");
  `);
  testRunner.log(`postMessage sent: ${postMessageSent !== undefined}`);

  const issue = await issuePromise;
  const dontCheckWarningStatus = ['isWarning'];
  testRunner.log(issue.params, 'Inspector issue: ', dontCheckWarningStatus);
  testRunner.completeTest();
})
