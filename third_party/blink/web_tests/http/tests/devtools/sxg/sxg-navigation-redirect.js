// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
(async function() {
  TestRunner.addResult('Tests the signed exchange information are available when the navigation succeeded after redirect.\n');
  await TestRunner.showPanel('network');
  NetworkTestRunner.networkLog().reset();
  const url =
      'http://localhost:8000/resources/redirect.php?url=' +
      encodeURIComponent(
        'http://127.0.0.1:8000/loading/sxg/resources/sxg-location.sxg');
  await TestRunner.addIframe(url);
  await ConsoleTestRunner.dumpConsoleMessages();
  NetworkTestRunner.dumpNetworkRequestsWithSignedExchangeInfo();
  TestRunner.completeTest();
})();
