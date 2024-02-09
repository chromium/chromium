// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests "Bypass for network" checkbox works with navigations. crbug.com/746220\n`);
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');
  await TestRunner.evaluateInPagePromise(`
        function getIframeBodyText(id) {
          return document.getElementById(id).contentWindow.document.body.innerText;
        }
    `);

  const scriptURL = 'resources/service-workers-bypass-for-network-navigation-worker.js';
  const scope = 'resources/service-workers-bypass-for-network-navigation-iframe.html';
  const frameId1 = 'frame_id1';
  const frameId2 = 'frame_id2';

  function loadIframe(frame_id) {
    TestRunner.addResult('Loading an iframe.');
    return TestRunner.addIframe(scope, {id: frame_id})
        .then(() => {
          TestRunner.addResult('The iframe loaded.');
          return TestRunner.callFunctionInPageAsync('getIframeBodyText', [frame_id]);
        })
        .then((data) => {
          TestRunner.addResult(' body: ' + data);
        });
  }

  ApplicationTestRunner.registerServiceWorker(scriptURL, scope)
      .then(_ => ApplicationTestRunner.waitForActivated(scope))
      .then(() => {
        return loadIframe('frame_id1');
      })
      .then(() => {
        TestRunner.addResult('Enable BypassServiceWorker.');
        Common.Settings.settingForTest('bypass-service-worker').set(true);
        return loadIframe('frame_id2');
      })
      .then(() => {
        TestRunner.addResult('Disable BypassServiceWorker.');
        Common.Settings.settingForTest('bypass-service-worker').set(false);
        return loadIframe('frame_id3');
      })
      .then(() => {
        TestRunner.completeTest();
      });
})();
