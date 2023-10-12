// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests disabling cache from inspector and seeing that preloads are not evicted from memory cache.\n`);
  await TestRunner.showPanel('network');

  await TestRunner.navigatePromise('resources/network-disable-cache-preloads.php');
  await TestRunner.NetworkAgent.setCacheDisabled(true);
  TestRunner.reloadPage(step2);

  function step2(msg) {
    ConsoleTestRunner.addConsoleSniffer(done);
    TestRunner.evaluateInPage('scheduleScriptLoad()');
  }

  async function done(msg) {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
