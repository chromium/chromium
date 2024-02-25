// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Verify that iframe's main resource is reported only once.\n`);
  await TestRunner.showPanel('resources');
  await TestRunner.evaluateInPageAsync(`
      (function createIframe() {
          var iframe = document.createElement('iframe');
          iframe.setAttribute('src', '${TestRunner.url('resources/dummy-iframe.html')}');
          document.body.appendChild(iframe);
          return new Promise(x => iframe.onload = x);
      })();
  `);

  var resourceTreeModel = new SDK.ResourceTreeModel.ResourceTreeModel(TestRunner.mainTarget);
  var resources = [];
  resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.ResourceAdded, event => resources.push(event.data));
  resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.CachedResourcesLoaded, function() {
    resources.sort((a, b) => {
      if (a.url === b.url)
        return 0;
      return a.url < b.url ? -1 : 1;
    });
    TestRunner.addResult('Reported resources:');
    TestRunner.addResult(resources.map(r => '  ' + r.url).join('\n'));
    TestRunner.completeTest();
  });
})();
