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
      await this._session.evaluate(`window.location.href = '${url}'`);
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
  };

  return (dp, session) => {
    return new FetchExtraInfoHelper(dp, session);
  };
})()
