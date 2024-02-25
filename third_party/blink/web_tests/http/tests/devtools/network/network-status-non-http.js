// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Network from 'devtools/panels/network/network.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Test network status of non http request.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function addScriptElement(src)
      {
          var element = document.createElement("script");
          element.src = src;
          document.body.appendChild(element);
      }

      function loadResources()
      {
          addScriptElement("network-status-non-http.js");
          addScriptElement("non-existent-file.js");

          // Test that data-url doesn't appear in network panel.
          addScriptElement("data:application/javascript,var x = 0;");
      }
  `);

  await TestRunner.NetworkAgent.setCacheDisabled(true);
  var requestsToWatch = /\/network-status-non-http\.js$|\/non-existent-file.js$|^data:application\/javascript,|/;
  var seenRequests = 0;

  function dumpRequests() {
    var logView = Network.NetworkPanel.NetworkPanel.instance().networkLogView;
    logView.refresh();
    var dataGrid = logView.element.querySelector('table.data');
    var urls = document.evaluate(
        '//tbody/tr/td[position()=1]/@title', dataGrid, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);
    var outputStrings = [];

    const requests = NetworkTestRunner.networkRequests();
    for (var request of requests) {
      var line = request.displayName + ':' + request.statusCode + ' ' + request.statusText;
      if (request.failed)
        line += '(failed)';
      else if (request.finished)
        line += 'Finished';
      TestRunner.addResult(line);
    }
    TestRunner.completeTest();
  }

  function onRequestFinished(request) {
    if (!requestsToWatch.test(request.url))
      return;
    if (++seenRequests === 3)
      setTimeout(dumpRequests);
  }

  TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestFinished, onRequestFinished);
  NetworkTestRunner.recordNetwork();
  TestRunner.evaluateInPage('loadResources()');
})();
