(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests sending of info for iframe with src which is being written to after a delay`);

  await dp.Page.enable();
  const frameIDs = new Set();

  page.navigate('http://devtools.test:8000/inspector-protocol/page/resources/iframe-src.html');

  let frame = (await dp.Page.onceFrameNavigated()).params.frame;
  testRunner.log(`Frame navigated to ${frame.url}`);
  frameIDs.add(frame.id);

  frame = (await dp.Page.onceFrameNavigated()).params.frame;
  testRunner.log(`Frame navigated to ${frame.url}`);
  frameIDs.add(frame.id);

  session.evaluate(`
    const doc = document.getElementById('frame').contentDocument;
    doc.open();
    doc.write("<h1>Hello world!</h1>");
    doc.close();
  `);

  frame = (await dp.Page.onceDocumentOpened()).params.frame;
  testRunner.log(`Document opened, new url: ${frame.url}`);
  frameIDs.add(frame.id);

  testRunner.log(`Received information about ${frameIDs.size} frame(s).`);
  testRunner.completeTest();
})
