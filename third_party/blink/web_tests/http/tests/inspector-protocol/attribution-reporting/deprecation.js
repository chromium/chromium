// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const testCases = [
    [
      'fetch',
      `fetch('/', {attributionReporting: {
         eventSourceEligible: true,
         triggerEligible: false,
       }})`,
    ],
    [
      'xhr',
      `const xhr = new XMLHttpRequest();
       xhr.open('GET', '/');
       xhr.setAttributionReporting({
         eventSourceEligible: true,
         triggerEligible: false,
       })`,
    ],
    // The `.name` is irrelevant for the test, but `evaluate` serializes the
    // result of the expression as JSON, which is not possible for the
    // WindowProxy returned by `open` itself.
    ['open', `open('/', '_blank', 'attributionsrc').name`],
    [
      '<img attributionsrc>',
      `document.body.innerHTML = '<img src="/" attributionsrc>'`
    ],
    [
      '<script attributionsrc>',
      `document.body.innerHTML = '<script src="/" attributionsrc>'`
    ],
    [
      '<a attributionsrc>',
      `document.body.innerHTML = '<a href="/" attributionsrc>';
       document.querySelector('a').click();`,
    ],
    [
      '<area attributionsrc>',
      `document.body.innerHTML = '<area href="/" attributionsrc>';
       document.querySelector('area').click();`,
    ],
    ['HTMLImageElement', `document.createElement('img').attributionSrc`],
    ['HTMLScriptElement', `document.createElement('script').attributionSrc`],
    ['HTMLAnchorElement', `document.createElement('a').attributionSrc`],
    ['HTMLAreaElement', `document.createElement('area').attributionSrc`],
  ];

  const {dp, page, session} = await testRunner.startBlank(
      'Test that Attribution Reporting API surfaces are deprecated.');
  await dp.Audits.enable();

  for (const [name, script] of testCases) {
    // At most one deprecation issue is reported per type per page, so
    // re-navigate to ensure that this limit is not reached.
    await page.navigate('/');

    session.evaluate(script);

    let issue;
    do {
      issue = await dp.Audits.onceIssueAdded();
    } while (issue.params.issue.code !== 'DeprecationIssue' ||
             issue.params.issue.details.deprecationIssueDetails.type !==
                 'AttributionReporting');

    testRunner.log(name);
  }

  testRunner.completeTest();
})
