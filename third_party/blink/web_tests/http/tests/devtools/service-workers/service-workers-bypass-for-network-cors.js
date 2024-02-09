// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests "Bypass for network" checkbox works with CORS requests. crbug.com/771742\n`);
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');
  await TestRunner.evaluateInPagePromise(`
      function takeInterceptedRequests(scope) {
        return new Promise((resolve) => {
            let channel = new MessageChannel();
            channel.port1.onmessage = msg => { resolve(JSON.stringify(msg.data)); };
            registrations[scope].active.postMessage({port: channel.port2}, [channel.port2]);
          });
      }

      function fetchInIframe(url, frame_id) {
        return document.getElementById(frame_id).contentWindow
            .fetch(url, {mode: 'cors'}).then((r) => r.text());
      }
      function xhrInIframe(url, frame_id) {
        return document.getElementById(frame_id).contentWindow.xhr(url);
      }
      function corsImageInIframe(url, frame_id) {
        return document.getElementById(frame_id).contentWindow.load_cors_image(url);
      }
  `);

  const scriptURL =
      'http://127.0.0.1:8000/devtools/service-workers/resources/service-workers-bypass-for-network-cors-worker.js';
  const scope =
      'http://127.0.0.1:8000/devtools/service-workers/resources/service-workers-bypass-for-network-cors-iframe.html';
  const target =
      'http://localhost:8000/devtools/service-workers/resources/service-workers-bypass-for-network-cors-target.php';
  const frameId = 'frame_id';

  function dumpInterceptedRequests() {
    return TestRunner.callFunctionInPageAsync('takeInterceptedRequests', [scope]).then((data) => {
      TestRunner.addResult('Intercepted requests:');
      JSON.parse(data).forEach((request) => {
        TestRunner.addResult(' url: ' + request.url);
        TestRunner.addResult(' mode: ' + request.mode);
      });
    });
  }

  function testCorsRequests(index) {
    TestRunner.addResult('CORS fetch(): ' + index);
    return TestRunner.callFunctionInPageAsync('fetchInIframe', [target + '?type=txt&fetch' + index, frameId])
        .then((data) => {
          if (data !== 'hello') {
            TestRunner.addResult('fetch response miss match: ' + data);
          }
          TestRunner.addResult('CORS XHR: ' + index);
          return TestRunner.callFunctionInPageAsync('xhrInIframe', [target + '?type=txt&xhr' + index, frameId]);
        })
        .then((data) => {
          if (data !== 'hello') {
            TestRunner.addResult('XHR response miss match: ' + data);
          }
          TestRunner.addResult('CORS image: ' + index);
          return TestRunner.callFunctionInPageAsync('corsImageInIframe', [target + '?type=img&img' + index, frameId]);
        });
  }

  ApplicationTestRunner.registerServiceWorker(scriptURL, scope)
      .then(_ => ApplicationTestRunner.waitForActivated(scope))
      .then(() => {
        TestRunner.addResult('Loading an iframe.');
        return TestRunner.addIframe(scope, {id: frameId});
      })
      .then(() => {
        TestRunner.addResult('The iframe loaded.');
        return testCorsRequests('1');
      })
      .then(() => {
        TestRunner.addResult('Enable bypassServiceWorker');
        Common.Settings.settingForTest('bypass-service-worker').set(true);
        return testCorsRequests('2');
      })
      .then(() => {
        TestRunner.addResult('Disable bypassServiceWorker');
        Common.Settings.settingForTest('bypass-service-worker').set(false);
        return testCorsRequests('3');
      })
      .then(() => {
        return dumpInterceptedRequests();
      })
      .then(() => {
        TestRunner.completeTest();
      });
})();
