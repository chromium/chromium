(function() {
  class FetchExtraInfoHelper {
    constructor(dp, session) {
      this._dp = dp;
      this._session = session;
    }

    async navigateWithExtraInfo(url) {
      const requestExtraInfoPromise = this._dp.Network.onceRequestWillBeSentExtraInfo();
      const responseExtraInfoPromise = this._dp.Network.onceResponseReceivedExtraInfo();
      await this._session.navigate(url);
      const requestExtraInfo = await requestExtraInfoPromise;
      const responseExtraInfo = await responseExtraInfoPromise;
      return {requestExtraInfo, responseExtraInfo};
    }

    async jsNavigateWithExtraInfo(url) {
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
      const requestExtraInfoPromise = this._dp.Network.onceRequestWillBeSentExtraInfo();
      const responseExtraInfoPromise = this._dp.Network.onceResponseReceivedExtraInfo();
      await this._session.evaluate(`fetch('${url}', {method: 'POST', credentials: 'include'})`);
      const requestExtraInfo = await requestExtraInfoPromise;
      const responseExtraInfo = await responseExtraInfoPromise;
      return {requestExtraInfo, responseExtraInfo};
    }

    async jsNavigateIFrameWithExtraInfo(iFrameId, url) {
      const promises = [this._dp.Network.onceRequestWillBeSent(), this._dp.Network.onceRequestWillBeSentExtraInfo(), this._dp.Network.onceResponseReceivedExtraInfo(), this._dp.Network.onceResponseReceived()];
      await this._session.evaluate(`document.getElementById('${iFrameId}').src = '${url}'`);
      return Promise.all(promises);
    }
  };

  return (dp, session) => {
    return new FetchExtraInfoHelper(dp, session);
  };
})()
