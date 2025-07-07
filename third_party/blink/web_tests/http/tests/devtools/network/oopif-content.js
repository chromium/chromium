// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as TextUtils from 'devtools/models/text_utils/text_utils.js';

(async function() {
  // TestRunner.startDumpingProtocolMessages();
  TestRunner.addResult(`Tests content is available for a cross-process iframe.\n`);
  await TestRunner.showPanel('network');
  NetworkTestRunner.recordNetwork();
  await TestRunner.addIframe('http://devtools.oopif.test:8000/devtools/resources/cross-origin-iframe.html');
  const request = NetworkTestRunner.networkRequests().pop();
  if (!request || !request.finished) {
    TestRunner.addResult('FAILED: request not finished!');
    TestRunner.completeTest();
    return;
  }
  const { content, error, isEncoded } = await request.requestContentData().then(TextUtils.ContentData.ContentData.asDeferredContent);
  TestRunner.addResult(`content: ${content}`);
  TestRunner.completeTest();
})();
