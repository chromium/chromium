// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that resources have proper documentURL set in the tree model.\n`);
  await TestRunner.showPanel('resources');

  TestRunner.resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.FrameNavigated, waitForResources);
  TestRunner.evaluateInPagePromise(`
    (function loadIframe() {
      var iframe = document.createElement("iframe");
      iframe.src = "${TestRunner.url('resources/resource-tree-reload-iframe.html')}";
      document.body.appendChild(iframe);
    })();
  `);

  async function waitForResources() {
    TestRunner.resourceTreeModel.removeEventListener(SDK.ResourceTreeModel.Events.FrameNavigated, waitForResources);
    await Promise.all([
      TestRunner.waitForUISourceCode('resource-tree-reload-iframe.html'),
      TestRunner.waitForUISourceCode('script-initial.js'),
      TestRunner.waitForUISourceCode('styles-initial-2.css'),
    ]);
    ApplicationTestRunner.dumpResources(resource => resource.url + ' => ' + resource.documentURL);
    TestRunner.completeTest();
  }
})();
