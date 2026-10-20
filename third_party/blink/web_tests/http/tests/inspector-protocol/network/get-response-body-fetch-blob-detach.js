(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that the load finishes when the inspector detaches while a body consumed as a blob is being downloaded.`);

  // This session never enables Network domain and stays attached.
  const pageSession = await page.createSession();
  await dp.Network.enable();

  // Sends the headers with the first bytes and the rest of the body half a
  // second later.
  const url = '/inspector-protocol/resources/data-xfer-resource.php?size=1000' +
      '&flush_header_with_x_bytes=100&flush_every=1000' +
      '&wait_after_headers_packet=500';

  await pageSession.evaluateAsync(async url => {
    const response = await fetch(url);
    window.blobPromise = response.blob();
  }, url);
  await session.disconnect();
  // The resource timing entry is only added once the load has finished.
  const result = await pageSession.evaluateAsync(async url => {
    const blob = await window.blobPromise;
    const entries = performance.getEntriesByName(new URL(url, location).href);
    return `blob size: ${blob.size}, resource timing entries: ${
        entries.length}`;
  }, url);
  testRunner.log(result);

  testRunner.completeTest();
})
