(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      `http://127.0.0.1:8000/inspector-protocol/network/resources/page-with-csp.php?csp=connect-src%20'self'`,
      `Tests for Network.loadNetworkResource with CSP`);

  const {result: {frameTree}} = await dp.Page.getFrameTree();
  const frameId = frameTree.frame.id;

  async function loadResource(url, explanation) {
    const response = await dp.Network.loadNetworkResource({
      frameId,
      url,
      options: {disableCache: false, includeCredentials: false}
    });
    testRunner.log(response.error ?? response.result, explanation, ['headers']);
  }

  const crossOriginUrl =
      `https://localhost:8443/inspector-protocol/network/resources/source.map`;
  await loadResource(
      crossOriginUrl,
      `Response for fetch with cross-origin resource (should be blocked by CSP):`);

  const sameOriginUrl =
      `http://127.0.0.1:8000/inspector-protocol/network/resources/source.map`;
  await loadResource(
      sameOriginUrl,
      `Response for fetch with same-origin resource (should be allowed by CSP):`);

  testRunner.completeTest();
})
