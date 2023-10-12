// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
(async function() {
  TestRunner.addResult('Tests the transfer size of signed exchange is set correctly.\n');
  await TestRunner.showPanel('network');
  NetworkTestRunner.networkLog().reset();
  await TestRunner.addIframe('/loading/sxg/resources/sxg-larger-than-10k.sxg');
  await ConsoleTestRunner.dumpConsoleMessages();
  NetworkTestRunner.dumpNetworkRequestsWithSignedExchangeInfo();
  var requests =
      NetworkTestRunner.findRequestsByURLPattern(/sxg-larger-than-10k.sxg/)
          .filter((e, i, a) => i % 2 == 0);
  TestRunner.assertTrue(requests.length === 1);
  TestRunner.assertTrue(requests[0].transferSize > 10000);
  TestRunner.completeTest();
})();
