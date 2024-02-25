// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests requests loaded from disk cache have correct timing\n`);
  await TestRunner.showPanel('network');
  await TestRunner.addScriptTag('resources/gc.js');
  await TestRunner.evaluateInPagePromise(`
      var scriptElement;
      function loadScript()
      {
          scriptElement = document.createElement("script");
          scriptElement.src = "resources/cached-script.php";
          document.head.appendChild(scriptElement);
      }

      function unloadScript()
      {
          scriptElement.parentElement.removeChild(scriptElement);
      }
  `);

  var timeZero = 0;

  NetworkTestRunner.recordNetwork();
  TestRunner.NetworkAgent.setCacheDisabled(true).then(step1);

  function step1() {
    ConsoleTestRunner.addConsoleSniffer(step2);
    TestRunner.evaluateInPage('loadScript()');
  }

  function step2(event) {
    TestRunner.evaluateInPage('unloadScript()', step3);
  }

  function step3() {
    TestRunner.evaluateInPage('gc()', step4);
  }

  function step4() {
    TestRunner.NetworkAgent.setCacheDisabled(true).then(step5);
  }

  function step5() {
    var request = NetworkTestRunner.networkRequests().pop();
    TestRunner.addResult('URL:' + request.url());
    TestRunner.addResult('from memory cache: ' + !!request.fromMemoryCache);
    TestRunner.addResult('from disk cache: ' + !!request.fromDiskCache);
    TestRunner.addResult('has timing: ' + !!request.timing);
    TestRunner.addResult('');
    timeZero = request.timing.requestTime;
    TestRunner.NetworkAgent.setCacheDisabled(false).then(step6);
  }

  function step6() {
    ConsoleTestRunner.addConsoleSniffer(step7);
    TestRunner.evaluateInPage('loadScript()');
  }

  function step7() {
    var request = NetworkTestRunner.networkRequests().pop();
    TestRunner.addResult('URL:' + request.url());
    TestRunner.addResult('from memory cache: ' + !!request.fromMemoryCache);
    TestRunner.addResult('from disk cache: ' + !!request.fromDiskCache);
    TestRunner.addResult('has timing: ' + !!request.timing);
    TestRunner.addResult('');
    var time = request.timing.requestTime;
    TestRunner.addResult('Second request starts later than first: ' + (time > timeZero));
    TestRunner.completeTest();
  }
})();
