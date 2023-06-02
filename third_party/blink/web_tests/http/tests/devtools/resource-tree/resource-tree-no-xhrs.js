// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that XHRs are not added to resourceTreeModel. https://bugs.webkit.org/show_bug.cgi?id=60321\n`);
  await TestRunner.showPanel('resources');

  NetworkTestRunner.makeSimpleXHR('GET', 'resources/resource.php', false, step2);

  function step2() {
    var resource = TestRunner.resourceTreeModel.resourceForURL(
        'http://127.0.0.1:8000/devtools/resource-tree/resources/resource.php');
    TestRunner.assertTrue(!resource, 'XHR resource should not be added to resourceTreeModel.');
    TestRunner.completeTest();
  }
})();
