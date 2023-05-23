// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Verify that dynamically added resource has metadata.\n`);
  await TestRunner.showPanel('resources');
  var url = TestRunner.url('resources/script-with-constant-last-modified.php');
  await TestRunner.evaluateInPageAsync(`
    (function () {
      var script = document.createElement("script");
      script.type = "text/javascript";
      script.src = "${url}";
      document.body.appendChild(script);
      return new Promise(x => script.onload = x);
    })();
  `);

  var resource = TestRunner.resourceTreeModel.resourceForURL(url);
  if (!resource) {
    TestRunner.addResult('ERROR: Failed to find resource.');
    TestRunner.completeTest();
    return;
  }
  TestRunner.addResult('Last modified: ' + (resource.lastModified() ? resource.lastModified().toISOString() : null));
  TestRunner.addResult('Content size: ' + resource.contentSize());

  TestRunner.completeTest();
})();
