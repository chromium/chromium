(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/test-page.html',
      `Tests that CORB/ORB blocking gets reported as an issue to DevTools.`);

  await dp.Audits.enable();

  // Returns a promise that returns true if a ResponseWasBlockedByORB issue
  // was added. It expectes a "probe" issue for a fetch to `probe_url` in
  // order to determine (without a race condition) at what point in time the
  // issue should have been received.
  // We do some sanity checks on the issue parameters while we're here.
  function expectIssueAdded(issueHasBeenAdded) {
    return dp.Audits.onceIssueAdded().then(issue => {
      const genericIssueDetails = issue?.params?.issue?.details?.genericIssueDetails;
      if (!genericIssueDetails)
        throw new Error('Unexpected issue shape.');
      if (genericIssueDetails.errorType !== 'ResponseWasBlockedByORB')
        throw new Error('Unexpected issue errorType.');
      if (!genericIssueDetails?.request?.url)
        throw new Error('Unexpected issue shape.');
      if (genericIssueDetails.request.url.includes('probe.test'))
        return issueHasBeenAdded;
      return expectIssueAdded(true);
    });
  }

  function shouldReportCorbBlocking() {
    // This returns a promise which returns whether a DevTools issue (other
    // than for "probe.test") was added.
    return expectIssueAdded(false);
  }

  // In order to recognize whether the sub-test a completed, we'll always
  // follow up with a URL that definitely gets (C)ORB-blocked. We'll complete
  // the test when we receive an issue for that probe url. That gives us a
  // non-racy point in time where the previous url - which we want to test
  // for - must have been received.
  const probe_url = 'http://probe.test:8000/inspector-protocol/network/resources/nosniff.pl';
  function loadImageAndProbe(url) {
    return `
        probe = function() { new Image().src = '${probe_url}'; }
        img = new Image();
        img.src = '${url}';
        img.onerror = probe;
        img.onload = probe;
    `;
  }

  let blocked_urls = [
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/nosniff.pl',
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/simple-iframe.html',
  ];
  for (const url of blocked_urls) {
    session.evaluate(loadImageAndProbe(url));
    testRunner.log(
        `Blocking cross-site document at ${url}: ` +
        `shouldReportCorbBlocking=${await shouldReportCorbBlocking()}.`);
  }

  let blocked_unreported_urls = [
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/204.pl',
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/404.pl',
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/content-length-0.pl',
  ];
  for (const url of blocked_unreported_urls) {
    session.evaluate(loadImageAndProbe(url));
    testRunner.log(
        `Blocking, but not reporting cross-site document at ${url}: ` +
        `shouldReportCorbBlocking=${await shouldReportCorbBlocking()}.`);
  }

  let allowed_urls = [
    'http://127.0.0.1:8000/inspector-protocol/network/resources/nosniff.pl',
    'http://127.0.0.1:8000/inspector-protocol/network/resources/simple-iframe.html',
    'http://devtools.oopif.test:8000/inspector-protocol/network/resources/test.css',
  ]
  for (const url of allowed_urls) {
    session.evaluate(loadImageAndProbe(url));
    testRunner.log(
        `Allowing cross-site document at ${url}: ` +
        `shouldReportCorbBlocking=${await shouldReportCorbBlocking()}.`);
  }

  testRunner.completeTest();
})
