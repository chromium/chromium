(async function(testRunner) {
  const {page, dp} = await testRunner.startBlank(
      `Tests sending of info for iframe with srcdoc which is being written to after a delay`);

  await dp.Page.enable();
  const frameIDs = new Set();

  page.navigate('http://devtools.test:8000/inspector-protocol/page/resources/iframe-srcdoc-write-delay.html');

  let frame = (await dp.Page.onceFrameNavigated()).params.frame;
  testRunner.log(`Frame navigated to ${frame.url}`);
  frameIDs.add(frame.id);

  frame = (await dp.Page.onceFrameNavigated()).params.frame;
  testRunner.log(`Frame navigated to ${frame.url}`);
  frameIDs.add(frame.id);

  frame = (await dp.Page.onceDocumentOpened()).params.frame;
  testRunner.log(`Document opened, new url: ${frame.url}`);
  frameIDs.add(frame.id);

  testRunner.log(`Received information about ${frameIDs.size} frame(s).`);
  testRunner.completeTest();
})

