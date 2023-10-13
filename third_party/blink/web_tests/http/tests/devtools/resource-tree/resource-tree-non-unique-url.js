// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Application from 'devtools/panels/application/application.js';

(async function() {
  TestRunner.addResult(
      `Tests resources panel shows several resources with the same url if they were loaded with inspector already opened.\n`);
  await TestRunner.showPanel('resources');
  TestRunner.addSniffer(Application.ApplicationPanelSidebar.FrameTreeElement.prototype, 'appendResource', onResource, true);
  TestRunner.evaluateInPageAnonymously(`
    (function loadIframe() {
      var iframe = document.createElement("iframe");
      iframe.src = "${TestRunner.url('resources/resource-tree-non-unique-url-iframe.html')}";
      document.body.appendChild(iframe);
    })();
  `);

  var cssRequestsCount = 0;
  function onResource(resource) {
    if (resource.url.match(/\.css$/) && ++cssRequestsCount === 2) {
      TestRunner.addResult('Resources Tree:');
      ApplicationTestRunner.dumpResourcesTree();
      TestRunner.completeTest();
    }
  }
})();
