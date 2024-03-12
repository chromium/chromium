// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Network from 'devtools/panels/network/network.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests to ensure column names are matching data.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function sendXHRRequest() {
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "resources/empty.html?xhr");
          xhr.send();
      }
  `);

  var columnsToTest = [
    // Note that the expectation for the priority field is HighHigh, because in
    // the expanded view mode we show the initial priority and the requested
    // priority.
    'name', 'method', 'status', 'protocol', 'scheme', 'domain', 'remote-address', 'type', 'initiator', 'cookies',
    'set-cookies', 'priority', 'cache-control', 'connection', 'content-encoding', 'content-length', 'vary'
  ];

  // Setup
  NetworkTestRunner.recordNetwork();
  await NetworkTestRunner.clearNetworkCache();

  TestRunner.evaluateInPage('sendXHRRequest()');
  var request = await TestRunner.waitForEvent(
      SDK.NetworkManager.Events.RequestFinished, TestRunner.networkManager,
      request => request.name() === 'empty.html?xhr');
  var xhrNode = await NetworkTestRunner.waitForNetworkLogViewNodeForRequest(request);

  Network.NetworkPanel.NetworkPanel.instance().networkLogView.refresh();
  for (var columnName of columnsToTest) {
    const cell = xhrNode.createCell(columnName);
    // Cell may contain live locations that are unresolved.
    await TestRunner.waitForPendingLiveLocationUpdates();
    TestRunner.addResult(columnName + ': ' + cell.textContent);
  }
  TestRunner.completeTest();
})();
