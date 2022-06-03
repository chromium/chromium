(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests download is canceled when behavior is set to deny.');
  await dp.Browser.setDownloadBehavior({
    behavior: 'deny'
  });
  dp.Page.onDownloadWillBegin(event => {
    testRunner.log(event);
  });

  async function waitForDownloadAndDump() {
    const visitedStates = new Set();
    await new Promise(resolve => {
      dp.Page.onDownloadProgress(event => {
        if (visitedStates.has(event.params.state))
          return;
        visitedStates.add(event.params.state);
        testRunner.log(event);
        if (event.params.state === 'completed' || event.params.state === 'canceled')
          resolve();
      });
    });
  }

  await dp.Page.enable();
  testRunner.log('Downloading via a navigation: ');
  session.evaluate('location.href = "/devtools/network/resources/resource.php?download=1"');
  await waitForDownloadAndDump();
  testRunner.log('Downloading by clicking a link: ');
  session.evaluate(`
    const a = document.createElement('a');
    a.href = '/devtools/network/resources/resource.php';
    a.download = 'hello.text';
    document.body.appendChild(a);
    a.click();
  `);
  await waitForDownloadAndDump();
  testRunner.completeTest();
})
