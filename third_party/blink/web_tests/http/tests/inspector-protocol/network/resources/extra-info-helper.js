(function() {
  class FetchExtraInfoHelper {
    constructor(dp, session) {
      this._dp = dp;
      this._session = session;
      this._cacheDisabled = false;
    }

    async _ensureCacheDisabled() {
      if (this._cacheDisabled) {
        return;
      }
      // ExtraInfo events only fire for network requests, not disk-cache hits.
      await this._dp.Network.setCacheDisabled({cacheDisabled: true});
      this._cacheDisabled = true;
    }

    async navigateWithExtraInfo(url) {
      await this._ensureCacheDisabled();
      const requestExtraInfoPromise = this._dp.Network.onceRequestWillBeSentExtraInfo();
      const responseExtraInfoPromise = this._dp.Network.onceResponseReceivedExtraInfo();
      await this._session.navigate(url);
      const requestExtraInfo = await requestExtraInfoPromise;
      const responseExtraInfo = await responseExtraInfoPromise;
      return {requestExtraInfo, responseExtraInfo};
    }

    async jsNavigateWithExtraInfo(url) {
      await this._ensureCacheDisabled();
      const requestExtraInfoPromise = this._dp.Network.onceRequestWillBeSentExtraInfo();
      const responseExtraInfoPromise = this._dp.Network.onceResponseReceivedExtraInfo();
      const response = await this._session.protocol.Runtime.evaluate(
          {expression: `window.location.href = '${url}'`});
      if (response.error && response.error.message != 'Inspected target navigated or closed') {
        throw new Error(response.error.message || response.error);
      }
      const requestExtraInfo = await requestExtraInfoPromise;
      const responseExtraInfo = await responseExtraInfoPromise;
      return {requestExtraInfo, responseExtraInfo};
    }

    async fetchWithExtraInfo(url) {
      await this._ensureCacheDisabled();
      const requestExtraInfoPromise = this._dp.Network.onceRequestWillBeSentExtraInfo();
      const responseExtraInfoPromise = this._dp.Network.onceResponseReceivedExtraInfo();
      await this._session.evaluate(`fetch('${url}', {method: 'POST', credentials: 'include'})`);
      const requestExtraInfo = await requestExtraInfoPromise;
      const responseExtraInfo = await responseExtraInfoPromise;
      return {requestExtraInfo, responseExtraInfo};
    }

    async jsNavigateIFrameWithExtraInfo(iFrameId, url) {
      await this._ensureCacheDisabled();
      const promises = [this._dp.Network.onceRequestWillBeSent(), this._dp.Network.onceRequestWillBeSentExtraInfo(), this._dp.Network.onceResponseReceivedExtraInfo(), this._dp.Network.onceResponseReceived()];
      await this._session.evaluate(`document.getElementById('${iFrameId}').src = '${url}'`);
      return Promise.all(promises);
    }
  };

  return (dp, session) => {
    return new FetchExtraInfoHelper(dp, session);
  };
})()
