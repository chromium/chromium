// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that WebSocketFrames errors are visible to Web Inspector.\n`);
  await TestRunner.evaluateInPagePromise(`
      var ws;
      function sendMessages() {
          ws = new WebSocket("ws://localhost:8000/does_not_exist");
          ws.onclose = function()
          {
              debug("Closed.");
          };
      }
  `);

  function onRequest(event) {
    var request = event.data;
    if (request.resourceType().name() === 'websocket') {
      var websocketFrames = request.frames();
      for (var i = 0; i < websocketFrames.length; i++) {
        var frame = websocketFrames[i];
        var result = Platform.StringUtilities.sprintf('%d-%s: %s', (i + 1), frame.type, frame.text);
        TestRunner.addResult(result);
        if (frame.type == SDK.NetworkRequest.WebSocketFrameType.Error)
          TestRunner.completeTest();
      }
    }
  }
  TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestUpdated, onRequest);
  TestRunner.evaluateInPage('sendMessages()');
})();
