// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as TextUtils from 'devtools/models/text_utils/text_utils.js';

(async function() {
  TestRunner.addResult(`Tests disabling cache from inspector.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.navigatePromise('resources/random-script-page.html');

  let status1;
  let content1;
  let status2;
  let content2;
  let status3;
  let content3;
  function loadScriptAndGetStatusAndContent(callback) {
    NetworkTestRunner.recordNetwork();
    ConsoleTestRunner.addConsoleSniffer(scriptLoaded);
    TestRunner.evaluateInPage('scheduleScriptLoad()');

    function scriptLoaded() {
      const request = NetworkTestRunner.networkRequests().pop();
      request.requestContentData().then(TextUtils.ContentData.ContentData.asDeferredContent).then(
        ({content, error, isEncoded}) => {
          callback({status: request?.statusCode, content, error, isEncoded});
        }
      );
    }
  }

  loadScriptAndGetStatusAndContent(step1);

  function step1({ status, content, error, isEncoded }) {
    content1 = content;
    status1 = status;
    TestRunner.reloadPage(step2);
  }

  function step2(msg) {
    loadScriptAndGetStatusAndContent(step3);
  }

  function step3({ status, content, error, isEncoded }) {
    content2 = content;
    status2 = status;
    TestRunner.NetworkAgent.invoke_setCacheDisabled({cacheDisabled: true}).then(step4);
  }

  function step4() {
    TestRunner.reloadPage(step5);
  }

  function step5() {
    loadScriptAndGetStatusAndContent(step6);
  }

  function step6({ status, content, error, isEncoded }) {
    content3 = content;
    status3 = status;

    TestRunner.assertEquals(status1, 200, 'Second script load should be from cache');
    TestRunner.assertEquals(status2, 304, 'Second script load should be from cache');
    TestRunner.assertTrue(content1 !== content3, 'First and third scripts should differ.');
    TestRunner.assertEquals(status3, 200, 'Third script load should be from network');
    TestRunner.NetworkAgent.invoke_setCacheDisabled({cacheDisabled: false}).then(step7);
  }

  function step7() {
    TestRunner.completeTest();
  }
})();
