// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Host from 'devtools/core/host/host.js';

(async function() {
  TestRunner.addResult(`Test loading file resource from front-end \n`);

  function testLoadForURL(url, next) {
    var fullURL = TestRunner.url(url);
    TestRunner.addResult('Loading resource from ' + url);

    Host.ResourceLoader.load(fullURL, null, callback);

    function callback(statusCode, headers, content) {
      if (!content)
        TestRunner.addResult('Failed: ' + statusCode);
      else {
        TestRunner.addResult('Content: ' + content);
      }
      TestRunner.deprecatedRunAfterPendingDispatches(next);
    }
  }

  TestRunner.runTestSuite([
    function testResourceFromFileScheme(next) {
      testLoadForURL('resources/file-resource-to-load-for-frontend.txt', next);
    },
  ]);
})();
