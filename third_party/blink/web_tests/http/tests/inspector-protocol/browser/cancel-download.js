(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Tests for browser.cancelDownload');

  await dp.Page.enable();

  const guidStateSeen = {};
  let currGuid = null;

  async function waitForState(state) {
    while (true) {
      if (guidStateSeen[[currGuid, state]]) {
        break;
      }
      await dp.Page.onceDownloadProgress();
    }
  }

  dp.Page.onDownloadWillBegin(event => {
    currGuid = event.params.guid;
    guidStateSeen[[currGuid, 'willBegin']] = true;
    testRunner.log(event, "Page.downloadWillBegin: ");
  });

  dp.Page.onDownloadProgress(event => {
    currGuid = event.params.guid;
    // Will log only once for the `inProgress` state
    if (!guidStateSeen[[currGuid, event.params.state]]) {
      guidStateSeen[[currGuid, event.params.state]] = true;
      testRunner.log(event, "Page.downloadProgress (change of state): ");
    }
  });

  let cdpResult = null;
  testRunner.log("## Expected usage");
  // Send a relatively large payload to make sure we trigger the download event without the presence
  // of the content-length header.
  session.evaluate('location.href = "/devtools/network/resources/resource.php?download=1&tail_wait=10000&size=4096"');
  await waitForState("inProgress");
  cdpResult = await dp.Browser.cancelDownload({guid: currGuid});
  testRunner.log(cdpResult, "Browser.cancelDownload: ");
  await waitForState("canceled");

  testRunner.log("## Trying already terminated downloads");
  // This shall finish silently as Chrome's download handler won't raise any error in such cases.
  cdpResult = await dp.Browser.cancelDownload({guid: currGuid});
  testRunner.log(cdpResult, "Browser.cancelDownload: ");

  testRunner.log("## Trying invalid GUID");
  cdpResult = await dp.Browser.cancelDownload({guid: "foo-no-such-guid"});
  testRunner.log(cdpResult, "Browser.cancelDownload: ");

  testRunner.completeTest();
})
