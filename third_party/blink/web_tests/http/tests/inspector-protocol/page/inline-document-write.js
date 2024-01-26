(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Tests that inline document.write() does not send Page.documentOpened event`);

  await dp.Page.enable();

  dp.Page.onFrameNavigated((event) => {
    testRunner.log(`Frame navigated to ${event.params.frame.url}`);
  });
  dp.Page.onDocumentOpened((event) => {
    testRunner.log(`Document opened, new url: ${event.params.frame.url}`);
  });
  dp.Page.onFrameStoppedLoading((event) => {
    testRunner.log('Frame stopped loading');
    testRunner.completeTest();
  });

  await page.navigate('http://devtools.test:8000/inspector-protocol/page/resources/inline-document-write.html');
})
