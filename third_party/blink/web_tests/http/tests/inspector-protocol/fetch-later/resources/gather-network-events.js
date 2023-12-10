(async function gatherNetworkEvents(testRunner, dp, options = {}) {
  let {
    log,
    requests: requestsRemaining,
    expectedRedirect
  } = {log: true, requests: 1, expectedRedirect: false, ...options};

  const events = [];
  const extraInfoEvents = [];

  const stabilizeNames = expectedRedirect ?
      [
        ...TestRunner.stabilizeNames, 'connectionId', 'timing', 'connectTiming',
        'wallTime', 'responseTime', 'securityDetails', 'remoteIPAddress',
        'Date', 'ETag', 'Last-Modified', 'User-Agent', 'X-Powered-By',
        'headersText'
      ] :
      [
        ...TestRunner.stabilizeNames, 'wallTime', 'requestTime', 'responseTime',
        'Date', 'receiveHeadersStart', 'receiveHeadersEnd', 'sendStart',
        'sendEnd', 'ETag', 'Last-Modified', 'User-Agent', 'headersText',
        'connectionId', 'X-Powered-By'
      ];

  await dp.Network.enable();
  dp.Network.onRequestWillBeSent(event => events.push(event));
  dp.Network.onRequestWillBeSentExtraInfo(event => extraInfoEvents.push(event));
  dp.Network.onResponseReceived(event => events.push(event));
  dp.Network.onResponseReceivedExtraInfo(event => extraInfoEvents.push(event));

  await new Promise(resolve => {
    function requestComplete() {
      if (--requestsRemaining === 0)
        resolve();
    }
    dp.Network.onLoadingFinished(event => {
      events.push(event);
      requestComplete();
    });
    dp.Network.onLoadingFailed(event => {
      events.push(event);
      requestComplete();
    });
  });

  // Sort the events by request ID, preserving the order in which requests were
  // first seen in the main event stream, and the relative order of events for a
  // particular request ID.
  const requestOrder = Array.from(new Set(events.map(e => e.params.requestId)));
  const byRequestOrder = (a, b) => requestOrder.indexOf(a.params.requestId) -
      requestOrder.indexOf(b.params.requestId);
  events.sort(byRequestOrder);
  extraInfoEvents.sort(byRequestOrder);

  if (log) {
    for (const event of events) {
      testRunner.log(event.params, event.method, stabilizeNames);
    }
  }

  return events;
})
