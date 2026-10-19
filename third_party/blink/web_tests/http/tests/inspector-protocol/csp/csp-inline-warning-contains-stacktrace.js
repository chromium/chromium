// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<meta http-equiv='Content-Security-Policy' content="script-src 'self'">`,
      `Tests that the Log.entryAdded CSP violation for an inline script injected from JavaScript contains a stack trace.`);

  const helper = await testRunner.loadScript('resources/csp-console-helper.js');
  await helper.collectViolations(testRunner, session, dp, 1, `
    function thisTest() {
      var s = document.createElement('script');
      s.innerText = "console.log(1)";
      document.head.appendChild(s);
    }
    thisTest();
  `);
  testRunner.completeTest();
});
