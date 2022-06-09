(function() {
class NetworkLifecycleObserver {
  constructor(dp) {
    this._dp = dp;
    this._dp.Network.enable();
    this._dp.Fetch.enable();
    this._dp.Fetch.onRequestPaused(e => {
      this._dp.Fetch.continueRequest({
        requestId: e.params.requestId,
      });
    });
  }

  async waitForCompletion(urlPattern) {
    const [willBeSentRequestId, pausedNetworkId, responseReceievedEvent] =
        await Promise.all([
          this._dp.Network
              .onceRequestWillBeSent(
                  evt => urlPattern.test(evt.params.request?.url))
              .then(evt => evt.params.requestId)
              .then(async requestId => {
                await Promise.race([
                  this._dp.Network.onLoadingFinished(
                      evt => evt.params.requestId === requestId),
                  this._dp.Network.onLoadingFailed(
                      evt => evt.params.requestId === requestId),
                ]);
                return requestId;
              }),
          this._dp.Fetch
              .onceRequestPaused(
                  evt => urlPattern.test(evt.params.request?.url))
              .then(evt => evt.params.networkId),
          this._dp.Network
              .onceResponseReceived(
                  evt => urlPattern.test(evt.params.response?.url))
              .then(evt => evt),
        ]);

    let logs = [];

    const {requestId: responseReceivedRequestId, response: {status}} =
        responseReceievedEvent.params;
    logs.push(`[${urlPattern}] Status Code ${status}`);

    if (willBeSentRequestId && willBeSentRequestId === pausedNetworkId &&
        pausedNetworkId === responseReceivedRequestId) {
      logs.push(`[${
          urlPattern}] OK: All expected network events found and align with one another!`)
    } else {
      logs.push(`[${
          urlPattern}] FAIL: requestId was empty or did not align with other network events. Network.requestWillBeSent.params.requestId (${
          willBeSentRequestId}), Fetch.requestPaused.params.networkId (${
          pausedNetworkId}), Network.responseReceived.params.requestId (${
          responseReceivedRequestId})`);
    }

    return logs.join('\n');
  }
}

return NetworkLifecycleObserver;
})()
