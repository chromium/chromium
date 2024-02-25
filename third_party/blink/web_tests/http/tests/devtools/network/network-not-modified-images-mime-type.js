// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that if an image is cached and the server responds with 304, the image MIME type is shown correctly.\n`);
  await TestRunner.showPanel('network');
  NetworkTestRunner.recordNetwork();
  await TestRunner.navigatePromise('resources/cached-image.html');
  const requests = NetworkTestRunner.networkRequests();
  const imageRequest = requests[requests.length - 1];
  TestRunner.addResult(`${imageRequest.url()} ${imageRequest.statusCode} ${imageRequest.mimeType}`);
  TestRunner.reloadPage(function () {
    const requests = NetworkTestRunner.networkRequests();
    const imageRequest = requests[requests.length - 1];
    TestRunner.addResult(`${imageRequest.url()} ${imageRequest.statusCode} ${imageRequest.mimeType}`);
    TestRunner.completeTest();
  });
})();
