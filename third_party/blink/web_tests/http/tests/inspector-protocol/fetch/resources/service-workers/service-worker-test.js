async (testRunner, {
  description,
  type,
  update,
  redirectTo,
  contentType,
  responseCode,
  serviceWorkerAllowedHeader
}) => {
  // testRunner.startDumpingProtocolMessages();
  const {session, dp} =
      await testRunner.startBlank(`Tests Service Worker (${type}) ${
          update ?
              'Update Registration' :
              'Initial Registration'} emits Network Events for ${description}`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  await session.navigate('./resources/empty.html');

  const attachedPromise = dp.Target.onceAttachedToTarget();

  const params = new URLSearchParams();
  if (update)
    params.set('defer', self.crypto.randomUUID());
  if (contentType)
    params.set('content_type', contentType);
  if (redirectTo)
    params.set('redirect_to', redirectTo);
  if (responseCode)
    params.set('response_code', responseCode);
  if (serviceWorkerAllowedHeader)
    params.set('service_worker_allowed_header', serviceWorkerAllowedHeader);
  const url = `./service-workers/worker.php?${params}`;

  const registeredPromise =
      session.evaluateAsync(`navigator.serviceWorker.register('${
          url}', { type: '${type}' }).then(r => {
          window.registration = r;
          return 'SUCCESS!';
        }).catch(e => 'FAILURE: ' + e)`);
  const attachedToTarget = await attachedPromise;
  const swdp = session.createChild(attachedToTarget.params.sessionId).protocol;
  const networkEvents = [
    swdp.Network.onceRequestWillBeSent(),
    swdp.Fetch.onceRequestPaused().then(evt => {
      swdp.Fetch.continueRequest({
        requestId: evt.params.requestId,
      });
      return evt;
    }),
    swdp.Network.onceResponseReceived(),
    swdp.Network.onceLoadingFinished(),
  ];

  await Promise.all([
    swdp.Network.enable(),
    swdp.Fetch.enable(),
    swdp.Runtime.runIfWaitingForDebugger(),
  ]);

  const [requestWillBeSent, requestPaused, responseReceived, loadingFinished] =
      await Promise.all(networkEvents);
  await registeredPromise;
  const idsMatch = requestWillBeSent.params.requestId &&
      requestWillBeSent.params.requestId === requestPaused.params.networkId &&
      requestWillBeSent.params.requestId ===
          responseReceived.params.requestId &&
      requestWillBeSent.params.requestId === loadingFinished.params.requestId;

  const redactUUID = (s) => s.replace(
      /[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}/, 'uuid');
  testRunner.log('==== INITIAL service worker request ====')
  testRunner.log(`Registration Result: ${redactUUID(await registeredPromise)}`);
  testRunner.log(`requestWillBeSent url: ${
      redactUUID(requestWillBeSent.params.request.url)}`);
  testRunner.log(
      `responseReceived status: ${responseReceived.params.response.status}`);
  testRunner.log(`requestIds match: ${idsMatch}`);

  if (update) {
    testRunner.log('==== UPDATE service worker request ====')
    // TODO(crbug.com/1334900): verify network events for the update call, and
    // add another test to check interception of the update request works.
    const updatedPromise = await session.evaluateAsync(
        `window.registration.update().then(() => 'SUCCESSFULLY UPDATED!').catch(e => 'FAILURE: ' + e)`);
    testRunner.log(
        `Update Registration Result: ${redactUUID(await updatedPromise)}`);
  }
  await testRunner.completeTest();
};
