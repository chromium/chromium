(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Tests sending of info for iframe with srcdoc`);

  await dp.Page.enable();
  const frameIDs = new Set();

  dp.Page.onFrameNavigated((event) => {
    testRunner.log(`Frame navigated to ${event.params.frame.url}`);
    frameIDs.add(event.params.frame.id);
  });
  dp.Page.onDocumentOpened((event) => {
    testRunner.log(`Document opened, new url: ${event.params.frame.url}`);
    frameIDs.add(event.params.frame.id);
  });

  await page.navigate('http://devtools.test:8000/inspector-protocol/page/resources/iframe-srcdoc.html');
  testRunner.log(`Received information about ${frameIDs.size} frame(s).`);
  testRunner.completeTest();
})
