// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as ProtocolClient from 'devtools/core/protocol_client/protocol_client.js';

(async function() {
  TestRunner.addResult(`Tests that computed styles are cached across synchronous requests.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
          background-color: green;
      }
      </style>
      <div>
        <div id="inspected">Test</div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function updateStyle()
      {
          document.getElementById("style").textContent = "#inspected { color: red }";
      }
  `);

  ElementsTestRunner.nodeWithId('inspected', step1);
  var backendCallCount = 0;
  var nodeId;

  function onBackendCall(sessionId, domain, method, params) {
    if (method === 'CSS.getComputedStyleForNode' && params.nodeId === nodeId)
      ++backendCallCount;
  }

  function step1(node) {
    var callsLeft = 2;
    nodeId = node.id;
    TestRunner.addSniffer(ProtocolClient.InspectorBackend.SessionRouter.prototype, 'sendMessage', onBackendCall, true);
    TestRunner.cssModel.getComputedStyle(nodeId).then(styleCallback);
    TestRunner.cssModel.getComputedStyle(nodeId).then(styleCallback);
    function styleCallback() {
      if (--callsLeft)
        return;
      TestRunner.addResult('# of backend calls sent [2 requests]: ' + backendCallCount);
      TestRunner.evaluateInPage('updateStyle()', step2);
    }
  }

  function step2() {
    TestRunner.cssModel.getComputedStyle(nodeId).then(callback);
    function callback() {
      TestRunner.addResult('# of backend calls sent [style update + another request]: ' + backendCallCount);
      TestRunner.completeTest();
    }
  }
})();
