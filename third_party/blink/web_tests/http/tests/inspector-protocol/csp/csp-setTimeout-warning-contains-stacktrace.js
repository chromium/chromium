// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<meta http-equiv='Content-Security-Policy' content="script-src 'self' 'unsafe-inline'">`,
      `Tests that the Log.entryAdded CSP violations for evaluating a string with setTimeout contain a stack trace.`);

  const helper = await testRunner.loadScript('resources/csp-console-helper.js');
  await helper.collectViolations(testRunner, session, dp, 2, `
      function thisTest() {
          setTimeout('log("FAIL")', 0);
      }
      thisTest();
  `);
  testRunner.completeTest();
});
