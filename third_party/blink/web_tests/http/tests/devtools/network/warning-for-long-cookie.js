// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Checks that we show warning message for long cookie.\n`);
  await TestRunner.showPanel('network');

  NetworkTestRunner.makeFetch(
      'http://127.0.0.1:8000/devtools/network/resources/set-cookie.php?length=4096', {}, step1);

  function step1() {
    NetworkTestRunner.makeFetch(
        'http://127.0.0.1:8000/devtools/network/resources/set-cookie.php?length=4097', {}, step2);
  }

  function step2() {
    NetworkTestRunner.makeFetch(
        'http://127.0.0.1:8000/devtools/network/resources/set-two-cookies.php?length=4097', {}, step3);
  }

  function step3() {
    NetworkTestRunner.makeFetch(
        'http://127.0.0.1:8000/devtools/network/resources/set-two-cookies.php?length=3072', {});
  }

  await ConsoleTestRunner.waitForConsoleMessagesPromise(2);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
