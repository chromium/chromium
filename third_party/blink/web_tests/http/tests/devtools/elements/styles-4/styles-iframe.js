// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that proper (and different) styles are returned for body elements of main document and iframe.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
        #main { background:blue; }

      /*# sourceURL=styles-iframe.js*/
      </style>
      <div id="main"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function loadIframe()
      {
          var iframe = document.createElement("iframe");
          iframe.src = "../styles/resources/styles-iframe-data.html";
          document.getElementById("main").appendChild(iframe);
      }
  `);

  TestRunner.evaluateInPage('loadIframe()');
  ConsoleTestRunner.addConsoleSniffer(step0);

  function step0() {
    ElementsTestRunner.selectNodeAndWaitForStyles('main', step1);
  }

  async function step1() {
    TestRunner.addResult('Main frame style:');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    ElementsTestRunner.selectNodeAndWaitForStyles('iframeBody', step2);
  }

  async function step2() {
    TestRunner.addResult('iframe style:');
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
