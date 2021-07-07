// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.runAsyncTestSuite([async function testNoSocketOpenWhenOffline() {
    SDK.multitargetNetworkManager.setNetworkConditions(
        SDK.NetworkManager.OfflineConditions);

    TestRunner.addResult(await TestRunner.evaluateInPageAsync(`
        new Promise((resolve) => {
          const ws = new WebSocket('ws://localhost:8880/echo');
          let log = '';
          ws.onopen = () => {log += 'onopen '; ws.close(); };
          ws.onmessage = () => log += 'onmessage ';
          ws.onerror = () => log += 'onerror ';
          ws.onclose = () => {
            log += 'onclose ';
            resolve(log);
          };
        });`));
  }]);
})();
