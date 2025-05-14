(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank("Check FedCM request are getting captured in devtools");

  await dp.Network.enable();

  // Navigate to a test page to generate network requests.
  await page.navigate(
    "https://devtools.test:8443/inspector-protocol/fedcm/resources/dialog-shown-event.https.html"
  );

  // Create a Map to capture request and response params
  const urlByRequestId = new Map();
  const responseStatusByUrl = new Map();

  // Array to store SuccessUrls
  const networkLoadingFinishedUrls = [];
  // Map to store FailedUrls with Status
  const networkLoadingFailedUrlsWithStatus = new Map();

  // Cache loaderId & frameId
  let cachedLoaderId = "";
  let cachedFramedId = "";

  // Listen for all devtools network request sent events.
  dp.Network.onRequestWillBeSent(event => {
    const params = event.params;
    const loaderId = event.params.loaderId;
    const frameId = event.params.frameId;

    if ((cachedLoaderId === "") && (cachedFramedId === "")) {
      cachedLoaderId = loaderId;
      cachedFramedId = frameId;
    }

    if ((cachedLoaderId === loaderId) && (cachedFramedId === frameId)) {
      // Insert into the Map with key as params.requestId and value as params.request.url
      urlByRequestId.set(params.requestId, params.request.url);
    }
  });

  // Listen for all devtools network response received events.
  dp.Network.onResponseReceived(event => {
    const params = event.params;
    // Insert into the Map with key as params.response.url and value as status & status text
    let statusText = params.response.statusText;
    const status = params.response.status + '::' + statusText;
    const loaderId = event.params.loaderId;
    const frameId = event.params.frameId;
    if ((cachedLoaderId === loaderId) && (cachedFramedId === frameId)) {
      // Insert into the Map with key as params.response.url, and value as status
      responseStatusByUrl.set(params.response.url, status);
    }
  });

  // Listen for all devtools network loading finished events.
  dp.Network.onLoadingFinished(event => {
    const requestId = event.params.requestId;
    networkLoadingFinishedUrls.push(urlByRequestId.get(requestId));
  });

  // Listen for all devtools network loading failed events.
  dp.Network.onLoadingFailed(event => {
    const requestId = event.params.requestId;
    const errorText = event.params.errorText;
    networkLoadingFailedUrlsWithStatus.set(urlByRequestId.get(requestId), errorText);
  });

  // Enable FedCM domain
  await dp.FedCm.enable({disableRejectionDelay: true});

  // Trigger FedCM dialog
  const dialogPromise = session.evaluateAsync("triggerDialog()");

  let msg = await dp.FedCm.onceDialogShown();
  if (msg.error) {
    testRunner.log(msg.error);
  } else {
    testRunner.log(msg.params, "msg.params: ", ["dialogId"]);
    dp.FedCm.selectAccount({dialogId: msg.params.dialogId, accountIndex: 0});
    const token = await dialogPromise;
    testRunner.log("token: " + token);
  }

  const sortedResponseStatusByUrl = new Map([...responseStatusByUrl.entries()].sort((a, b) => a[0].localeCompare(b[0])));

  testRunner.log('urls loaded');
  networkLoadingFinishedUrls.sort();

  networkLoadingFinishedUrls.forEach((item, index) => {
    const status = sortedResponseStatusByUrl.get(item);
    testRunner.log(`Url: ${item}, Status: ${status}`);
  });

  testRunner.log('urls failed');
  const sortedFailedUrlsWithStatus = new Map([...networkLoadingFailedUrlsWithStatus.entries()].sort((a, b) => a[0].localeCompare(b[0])));
  sortedFailedUrlsWithStatus.forEach((value, key) => {
    testRunner.log(`Url: ${key}, Status: ${value}`);
  });

  testRunner.completeTest();
});