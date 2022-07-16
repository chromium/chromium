// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that injected user stylesheets are reflected in the Styles pane.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
        #main { background:blue; }
      </style>
      <div id="main"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function injectStyleSheet(context)
      {
          var styleSheet = "#main { color: red; border-style: solid; -webkit-border-image: url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAQAAAC1+jfqAAAAiElEQVR42r2RsQrDMAxEBRdl8SDcX8lQPGg1GBI6lvz/h7QyRRXV0qUULwfvwZ1tenw5PxToRPWMC52eA9+WDnlh3HFQ/xBQl86NFYJqeGflkiogrOvVlIFhqURFVho3x1moGAa3deMs+LS30CAhBN5nNxeT5hbJ1zwmji2k+aF6NENIPf/hs54f0sZFUVAMigAAAABJRU5ErkJggg==) }  #iframeBody { background: red }";
          if (context.testRunner)
              context.testRunner.insertStyleSheet(styleSheet);
      }

      injectStyleSheet(window);
      function loadIframe()
      {
          var iframe = document.createElement("iframe");
          iframe.src = "../styles/resources/inject-stylesheet-iframe-data.html";
          document.getElementById("main").appendChild(iframe);
      }
  `);

  ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('main', step0);

  async function step0() {
    TestRunner.addResult('Main frame style:');
    await ElementsTestRunner.dumpSelectedElementStyles();
    TestRunner.evaluateInPage('loadIframe()');
    ConsoleTestRunner.addConsoleSniffer(step1);
  }

  function step1() {
    ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('iframeBody', step2);
  }

  async function step2() {
    TestRunner.addResult('iframe style:');
    await ElementsTestRunner.dumpSelectedElementStyles();
    TestRunner.completeTest();
  }
})();
