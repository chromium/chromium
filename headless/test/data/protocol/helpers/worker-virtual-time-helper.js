// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A helper class to initialize virtual time and load a worker.
 */
(class WorkerVirtualTimeHelper {
  constructor(testRunner, session) {
    this.testRunner_ = testRunner;
    this.session_ = session;
    this.dp_ = session.protocol;
  }

  async createFetcher_() {
    const FetchHelper = await this.testRunner_.loadScriptAbsolute(
        '../fetch/resources/fetch-test.js');
    // Note we can't just use testRunner.browserP() since that session
    // is in a discovery-only mode, so re-attach.
    const { result: { sessionId } } =
        await this.testRunner_.browserP().Target.attachToBrowserTarget({});
    const { protocol: bp } = new TestRunner.Session(testRunner, sessionId);
    const fetcher = new FetchHelper(this.testRunner_, bp);
    await fetcher.enable();
    return {fetcher, FetchHelper};
  }

  async loadWorker(content) {
    const {fetcher, FetchHelper} = await this.createFetcher_();

    fetcher.onceRequest('http://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(`
          <html>
          <script>
            window.onload = function() {
              window.worker = new Worker('/worker.js')
            };
          </script>
          </html>
      `));
    fetcher.onceRequest('http://test.com/worker.js').fulfill(
      FetchHelper.makeContentResponse(content));

    await this.dp_.Emulation.setVirtualTimePolicy({
        policy: 'pause',
        initialVirtualTime: 100
    });
    await this.dp_.Page.navigate({url: 'http://test.com/index.html'});
    await this.dp_.Emulation.setVirtualTimePolicy({
        policy: 'pauseIfNetworkFetchesPending',
        budget: 1000});
    this.dp_.Target.setAutoAttach({
      autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
    const attached = (await this.dp_.Target.onceAttachedToTarget()).params;
    const wp = this.session_.createChild(attached.sessionId).protocol;
    return { wp, fetcher, FetchHelper };
  }
})
