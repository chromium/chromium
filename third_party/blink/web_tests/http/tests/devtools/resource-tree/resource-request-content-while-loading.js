// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as TextUtils from 'devtools/models/text_utils/text_utils.js';

(async function() {
  TestRunner.addResult(
      `Tests resource content is correctly loaded if Resource.requestContentData was called before network request was finished. https://bugs.webkit.org/show_bug.cgi?id=90153\n`);
  await TestRunner.showPanel('resources');

  TestRunner.addSniffer(SDK.ResourceTreeModel.ResourceTreeFrame.prototype, 'addRequest', requestAdded, true);
  TestRunner.addSniffer(TestRunner.PageAgent, 'invoke_getResourceContent', pageAgentGetResourceContentCalled, true);
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
    resource.requestContentData().then(TextUtils.ContentData.ContentData.asDeferredContent).then(contentLoaded);
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
