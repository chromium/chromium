(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
    `Verify that Selective Permissions Intervention issue is generated.\n`);

  await dp.Audits.enable();
  const adScriptUrl = testRunner.url('resources/trigger-geo.js');

  // Grant Geolocation via CDP.
  const { result: originRes } = await dp.Runtime.evaluate({ expression: 'location.origin' });
  await dp.Browser.grantPermissions({
    origin: originRes.value,
    permissions: ['geolocation']
  });

  // Configure the subresource filter and load the script which will be
  // tagged as ad related, and it will call the geolocation api, triggering
  // the intervention.
  session.evaluate(`
    testRunner.setHighlightAds();

    // Tag 'trigger-geo.js' as an ad resource but allow it to load
    testRunner.setDisallowedSubresourcePathSuffixes(["trigger-geo.js"], false);

    let ad_script = document.createElement('script');
    ad_script.src = "${adScriptUrl}";
    document.body.appendChild(ad_script);
  `);

  // Capture the intervention issue.
  const issue = await dp.Audits.onceIssueAdded();

  // Some of the returned keys are dynamic, so sanitize them for expectations.
  function sanitize(obj) {
    if (typeof obj !== 'object' || obj === null) return obj;
    if (Array.isArray(obj)) return obj.map(sanitize);
    const result = {};
    for (const key in obj) {
      if (key === 'url' || key === 'name') {
        result[key] = testRunner.trimURL(obj[key]);
      } else {
        result[key] = sanitize(obj[key]);
      }
    }
    return result;
  }

  testRunner.log(sanitize(issue.params), "Inspector issue: ");

  testRunner.completeTest();
})
