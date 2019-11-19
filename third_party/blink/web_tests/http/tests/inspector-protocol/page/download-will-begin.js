(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests we properly emit Page.downloadWillBegin.');

  async function waitForDownloadAndDump() {
    const params = (await dp.Page.onceDownloadWillBegin()).params;
    testRunner.log(params);
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
