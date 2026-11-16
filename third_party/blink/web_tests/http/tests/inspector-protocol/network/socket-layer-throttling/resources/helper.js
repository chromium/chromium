(function() {
// Round-trip time for the network conditions under test. Request durations are
// classified against half of it: a throttled request can't complete in less
// than the configured RTT, as its request and response each pay half of it,
// even on a reused connection. An unthrottled request to localhost takes a
// tiny fraction of it, so the classification has a wide margin either way.
// (Connect times are not used, as the socket pool's backup connect jobs leave
// throttled connections idle, for later requests to reuse.)
const RTT_MS = 1000;
const THRESHOLD_MS = RTT_MS / 2;

class SocketLayerThrottlingHelper {
  constructor(testRunner, session, dp) {
    this._testRunner = testRunner;
    this._session = session;
    this._dp = dp;
    this._requestCount = 0;
  }

  get rttMs() {
    return RTT_MS;
  }

  // Returns a unique path, so that each request is measured separately. Pages
  // resolve it against their own origin, to keep requests same-origin.
  resourcePath(type) {
    const url = new URL(this._testRunner.url(
        `resources/resource.php?type=${type}&request=${++this._requestCount}`));
    return url.pathname + url.search;
  }

  // Sets a single rule without a URL pattern, with the given conditions, which
  // default to no throttling, and returns the response.
  setConditions(conditions, dp = this._dp) {
    return this._logIfError(dp.Network.emulateNetworkConditionsByRule({
      matchedNetworkConditions: [{
        urlPattern: '',
        latency: 0,
        downloadThroughput: -1,
        uploadThroughput: -1,
        ...conditions,
      }],
    }));
  }

  // Clears the conditions.
  async clearConditions(dp = this._dp) {
    await this._logIfError(dp.Network.emulateNetworkConditionsByRule(
        {matchedNetworkConditions: []}));
  }

  // Fetches a new resource of the given type from the page, and returns the
  // request's duration.
  measureFetch(type = 'text') {
    return this.measureFetchOf(this.resourcePath(type));
  }

  // Fetches the resource at `path` from the page, and returns the request's
  // duration.
  measureFetchOf(path) {
    return this._session.evaluateAsync(async path => {
      const url = new URL(path, location.href).href;
      const response = await fetch(url);
      await response.arrayBuffer();
      return performance.getEntriesByName(url).pop().duration;
    }, path);
  }

  // Fetches the resource at `path` from the page, and returns the response's
  // status, or the error message if the fetch fails.
  fetchResult(path) {
    return this._session.evaluateAsync(async path => {
      try {
        const response = await fetch(new URL(path, location.href).href);
        await response.arrayBuffer();
        return response.status;
      } catch (error) {
        return error.message;
      }
    }, path);
  }

  // POSTs a request body of the given size from the page, and returns the
  // request's duration.
  measureUpload(bytes) {
    return this._session.evaluateAsync(async (path, bytes) => {
      const url = new URL(path, location.href).href;
      const response =
          await fetch(url, {method: 'POST', body: 'a'.repeat(bytes)});
      await response.arrayBuffer();
      return performance.getEntriesByName(url).pop().duration;
    }, this.resourcePath('text'), bytes);
  }

  // Loads a script element, and returns the request's duration.
  measureScript() {
    return this._session.evaluateAsync(async path => {
      const url = new URL(path, location.href).href;
      await new Promise((resolve, reject) => {
        const script = document.createElement('script');
        script.src = url;
        script.onload = resolve;
        script.onerror = reject;
        document.head.appendChild(script);
      });
      return performance.getEntriesByName(url).pop().duration;
    }, this.resourcePath('script'));
  }

  // Navigates the page to `origin`, by default its current one, and returns
  // the navigation's duration.
  async measureNavigation(origin) {
    origin ??= await this._session.evaluate(() => location.origin);
    await this._session.navigate(origin + this.resourcePath('html'));
    return this._session.evaluate(
        () => performance.getEntriesByType('navigation')[0].duration);
  }

  // Logs whether a request that took `ms` was throttled. The duration is only
  // logged when it doesn't match `expectThrottled`, to keep the expectations
  // stable while making failures easy to diagnose.
  logThrottling(label, ms, expectThrottled, thresholdMs = THRESHOLD_MS) {
    const throttled = ms >= thresholdMs;
    const details =
        throttled === expectThrottled ? '' : ` (${Math.round(ms)}ms)`;
    this._testRunner.log(
        `${label}: ${throttled ? 'throttled' : 'not throttled'}${details}`);
  }

  async _logIfError(responsePromise) {
    const response = await responsePromise;
    if (response.error) {
      this._testRunner.log(response.error, 'Unexpected error: ');
    }
    return response;
  }
}

return (testRunner, session, dp) =>
           new SocketLayerThrottlingHelper(testRunner, session, dp);
})()
