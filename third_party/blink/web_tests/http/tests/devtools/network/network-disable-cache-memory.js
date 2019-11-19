// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests disabling cache from inspector.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.navigatePromise('resources/random-script-page.html');

  var content1;
  var content2;
  var content3;

  function loadScriptAndGetContent(callback) {
    NetworkTestRunner.recordNetwork();
    ConsoleTestRunner.addConsoleSniffer(scriptLoaded);
    TestRunner.evaluateInPage('scheduleScriptLoad()');

    function scriptLoaded() {
      var request = NetworkTestRunner.networkRequests().pop();
      request.requestContent().then(callback);
    }
  }

  loadScriptAndGetContent(step1);

  function step1({ content, error, isEncoded }) {
    content1 = content;
    TestRunner.reloadPage(step2);
  }

  function step2(msg) {
    loadScriptAndGetContent(step3);
  }

  function step3({ content, error, isEncoded }) {
    content2 = content;
    TestRunner.NetworkAgent.setCacheDisabled(true).then(step4);
  }

  function step4() {
    TestRunner.reloadPage(step5);
  }

  function step5() {
    loadScriptAndGetContent(step6);
  }

  function step6({ content, error, isEncoded }) {
    content3 = content;

    TestRunner.assertTrue(content1 === content2, 'First and second scripts should be equal.');
    TestRunner.assertTrue(content2 !== content3, 'Second and third scripts should differ.');
    TestRunner.NetworkAgent.setCacheDisabled(false).then(step7);
  }

  function step7() {
    TestRunner.completeTest();
  }
})();
