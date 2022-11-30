// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `This test injects an inline script from JavaScript. The resulting console error should contain a stack trace.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');

  await TestRunner.loadHTML(`
    <!DOCTYPE html>
    <meta http-equiv='Content-Security-Policy' content="script-src 'self'">
  `);

  await TestRunner.evaluateInPagePromise(`
    function thisTest() {
      var s = document.createElement('script');
      s.innerText = "console.log(1)";
      document.head.appendChild(s);
    }
    thisTest();
  `);
  ConsoleTestRunner.dumpStackTraces();
  TestRunner.completeTest();
})();
