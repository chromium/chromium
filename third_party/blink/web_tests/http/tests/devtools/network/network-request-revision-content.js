// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  'use strict';
  TestRunner.addResult(
      `Tests how revision requests content if original content was not loaded yet. https://bugs.webkit.org/show_bug.cgi?id=63631\n`);
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
  Workspace.Workspace.WorkspaceImpl.instance().addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, step2);
  TestRunner.evaluateInPage('loadStylesheet()');

  let uiSourceCode;

  function step2(event) {
    var eventUISourceCode = event.data;
    if (eventUISourceCode.url().indexOf('style.css') == -1)
      return;
    var request = NetworkTestRunner.networkRequests().pop();
    uiSourceCode = Workspace.Workspace.WorkspaceImpl.instance().uiSourceCodeForURL(request.url());
    if (!uiSourceCode)
      return;
    uiSourceCode.addRevision('');
    uiSourceCode.requestContent().then(step3);
  }

  function step3({content}) {
    TestRunner.addResult(uiSourceCode.url());
    TestRunner.addResult(content);
    TestRunner.completeTest();
  }
})();
