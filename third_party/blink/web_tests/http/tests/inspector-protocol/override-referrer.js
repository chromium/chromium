(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that the navigation referrer can be overridden.`);

  function parseURL(url) {
    var result = {};
    var match = url.match(/^([^:]+):\/\/([^\/:]*)(?::([\d]+))?(?:(\/[^#]*)(?:#(.*))?)?$/i);
    if (!match)
      return result;
    result.scheme = match[1].toLowerCase();
    result.host = match[2];
    result.port = match[3];
    result.path = match[4] || "/";
    result.fragment = match[5];
    return result;
  }

  var referrers = [];
  await dp.Network.enable();
  dp.Network.onRequestWillBeSent(event => {
    var params = event.params;
    var referrer = params.request.headers.Referer;
    if (!referrer)
      return;

    referrers.push(parseURL(referrer).host);
    if (referrers.length === 2) {
      // Only log the list the found referrers at the end of the test.
      // Otherwise the first one will be lost because the target page is in
      // the middle of loading.
      testRunner.log('Referrers: ' + JSON.stringify(referrers));
      testRunner.completeTest();
    }
  });
  dp.Page.navigate({url: testRunner.url('resources/image.html'), referrer: 'http://referrer.com/'});
})
