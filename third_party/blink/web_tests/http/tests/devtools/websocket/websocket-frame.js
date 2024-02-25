// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that WebSocketFrames are being sent and received by Web Inspector.\n`);
  await TestRunner.evaluateInPagePromise(`
      var ws;
      function sendMessages() {
          ws = new WebSocket("ws://localhost:8880/echo");
          ws.onopen = function()
          {
              ws.send("test");
              ws.send("exit");
          };
      }
  `);

  var frames = [];
  function onRequest(event) {
    var request = event.data;
    var done = false;
    if (request.resourceType().name() !== 'websocket')
      return;
    var previous_frames = frames;
    frames = [];
    var websocketFrames = request.frames();
    for (var i = 0; i < websocketFrames.length; i++) {
      var frame = websocketFrames[i];
      frames[i] = Platform.StringUtilities.sprintf('%d-%s: %s', (i + 1), frame.type, frame.text);
      if (frame.type !== SDK.NetworkRequest.WebSocketFrameType.Send && frame.text === 'exit')
        done = true;
    }
    if (JSON.stringify(frames) === JSON.stringify(previous_frames)) {
      // There is no update.
      return;
    }
    for (var i = 0; i < frames.length; ++i)
      TestRunner.addResult(frames[i]);
    if (done)
      TestRunner.completeTest();
  }
  TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestUpdated, onRequest);
  TestRunner.evaluateInPage('sendMessages()');
})();
