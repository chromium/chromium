// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests resource content is correctly loaded if Resource.requestContent was called before network request was finished. https://bugs.webkit.org/show_bug.cgi?id=90153\n`);
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.showPanel('resources');

  TestRunner.addSniffer(SDK.ResourceTreeFrame.prototype, '_addRequest', requestAdded, true);
  TestRunner.addSniffer(TestRunner.PageAgent, 'getResourceContent', pageAgentGetResourceContentCalled, true);
  TestRunner.evaluateInPageAsync(`
    (function loadStylesheet() {
      var styleElement = document.createElement("link");
      styleElement.rel = "stylesheet";
      styleElement.href = "${TestRunner.url('resources/styles-initial.css')}";
      document.head.appendChild(styleElement);
    })();
  `);
  var contentWasRequested = false;
  var resource;

  function requestAdded(request) {
    if (request.url().indexOf('styles-initial') === -1)
      return;
    resource = ApplicationTestRunner.resourceMatchingURL('styles-initial');
    if (!resource || !resource.request || contentWasRequested) {
      TestRunner.addResult('Cannot find resource');
      TestRunner.completeTest();
    }
    resource.requestContent().then(contentLoaded);
    contentWasRequested = true;
  }

  function pageAgentGetResourceContentCalled() {
    if (!resource.request.finished) {
      TestRunner.addResult('Request must be finished before calling getResourceContent');
      TestRunner.completeTest();
    }
  }

  function contentLoaded({ content, error, isEncoded }) {
    TestRunner.addResult('Resource url: ' + resource.url);
    TestRunner.addResult('Resource content: ' + content);
    TestRunner.completeTest();
  }
})();
