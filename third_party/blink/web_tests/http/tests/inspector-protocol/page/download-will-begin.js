(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Tests we properly emit Page.downloadWillBegin.');

  await dp.Page.enable();

  await session.evaluateAsync(`
    const frame = document.createElement('iframe');
    frame.src = location.href;
    document.body.appendChild(frame);
    new Promise(resolve => frame.onload = resolve);
  `);

  const frameTree = (await dp.Page.getFrameTree()).result.frameTree;
  const frameNames = new Map([
    [frameTree.frame.id, 'top'],
    [frameTree.childFrames[0].frame.id, 'child']
  ]);

  dp.Page.onDownloadWillBegin(event => {
    const frame = frameNames.get(event.params.frameId) || 'unknown';
    testRunner.log(event, `downloadWillBegin from ${frame} frame: `);
  });

  async function waitForDownloadAndDump() {
    const visitedStates = new Set();
    await new Promise(resolve => {
      dp.Page.onDownloadProgress(event => {
        if (visitedStates.has(event.params.state))
          return;
        visitedStates.add(event.params.state);
        testRunner.log(event);
        if (event.params.state === 'completed')
          resolve();
      });
    });
  }

  async function waitForDownloadCompleted() {
    await dp.Page.onceDownloadProgress(event => event.params.state === 'completed');
  }

  testRunner.log('Downloading via a navigation: ');
  session.evaluate('location.href = "/devtools/network/resources/resource.php?download=1"');
  await waitForDownloadAndDump();

  testRunner.log('Downloading via a navigation of subframe: ');
  session.evaluate(`frames[0].frameElement.src = '/devtools/network/resources/resource.php?download=1'`);
  await waitForDownloadAndDump();

  function createLinkAndClick(doc) {
    const a = doc.createElement('a');
    a.href = '/devtools/network/resources/resource.php';
    a.download = 'hello.text';
    doc.body.appendChild(a);
    a.click();
  }

  testRunner.log('Downloading by clicking a link: ');
  session.evaluate(`(${createLinkAndClick})(document)`);
  await waitForDownloadAndDump();

  testRunner.log('Downloading by clicking a link in subframe: ');
  session.evaluate(`(${createLinkAndClick})(frames[0].document)`);
  await waitForDownloadAndDump();

  testRunner.log(
      'Downloading by clicking a link (no HTTP Content-Disposition header, a[download=""]): ');
  session.evaluate(`
    const blankDownloadAttr = document.createElement('a');
    blankDownloadAttr.href = '/devtools/network/resources/download.zzz';
    blankDownloadAttr.download = ''; // Intentionally left blank to trigger unamed download
    document.body.appendChild(blankDownloadAttr);
    blankDownloadAttr.click();
  `);
  await waitForDownloadCompleted();
  testRunner.log(
      'Downloading by clicking a link (HTTP Content-Disposition header with filename=foo.txt, no a[download]): ');
  session.evaluate(`
    const headerButNoDownloadAttr = document.createElement('a');
    headerButNoDownloadAttr.href = '/devtools/network/resources/resource.php?named_download=foo.txt';
    document.body.appendChild(headerButNoDownloadAttr);
    headerButNoDownloadAttr.click();
  `);
  await waitForDownloadCompleted();
  testRunner.log(
      'Downloading by clicking a link (HTTP Content-Disposition header with filename=override.txt, a[download="foo.txt"]): ');
  session.evaluate(`
    const headerAndConflictingDownloadAttr = document.createElement('a');
    headerAndConflictingDownloadAttr.href = '/devtools/network/resources/resource.php?named_download=override.txt';
    headerAndConflictingDownloadAttr.download = 'foo.txt';
    document.body.appendChild(headerAndConflictingDownloadAttr);
    headerAndConflictingDownloadAttr.click();
  `);
  await waitForDownloadCompleted();
  testRunner.log(
      'Downloading by clicking a link (HTTP Content-Disposition header without filename, no a[download]): ');
  session.evaluate(`
    const unnamedDownload = document.createElement('a');
    unnamedDownload.href = '/devtools/network/resources/resource.php?named_download';
    document.body.appendChild(unnamedDownload);
    unnamedDownload.click();
  `);
  await waitForDownloadCompleted();
  testRunner.log(
      'Downloading by clicking a link (HTTP Content-Disposition header without filename, no a[download], js): ');
  session.evaluate(`
    const jsDownload = document.createElement('a');
    jsDownload.href = '/devtools/network/resources/resource.php?named_download&type=js';
    document.body.appendChild(jsDownload);
    jsDownload.click();
  `);
  await waitForDownloadCompleted();
  testRunner.log(
      'Downloading by clicking a link (HTTP Content-Disposition header without filename, no a[download], image): ');
  session.evaluate(`
    const imageDownload = document.createElement('a');
    imageDownload.href = '/devtools/network/resources/resource.php?named_download&type=image';
    document.body.appendChild(imageDownload);
    imageDownload.click();
  `);
  await waitForDownloadCompleted();
  testRunner.completeTest();
})
