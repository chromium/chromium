// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests how revision requests content if original content was not loaded yet. https://bugs.webkit.org/show_bug.cgi?id=63631\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function loadStylesheet()
      {
          var css = document.createElement("link");
          css.rel = "stylesheet";
          css.type = "text/css";
          css.href = "resources/style.css";
          document.head.appendChild(css);
      }
  `);

  NetworkTestRunner.recordNetwork();
  Workspace.workspace.addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, step2);
  TestRunner.evaluateInPage('loadStylesheet()');

  var resource;
  function step2(event) {
    var eventUISourceCode = event.data;
    if (eventUISourceCode.url().indexOf('style.css') == -1)
      return;
    var request = NetworkTestRunner.networkRequests().pop();
    uiSourceCode = Workspace.workspace.uiSourceCodeForURL(request.url());
    if (!uiSourceCode)
      return;
    uiSourceCode.addRevision('');
    uiSourceCode.requestContent().then(step3);
  }

  function step3({ content, error, isEncoded }) {
    TestRunner.addResult(uiSourceCode.url());
    TestRunner.addResult(content);
    TestRunner.completeTest();
  }
})();
