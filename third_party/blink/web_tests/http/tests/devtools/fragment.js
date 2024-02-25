// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests fragment is stripped from url by resource and page agents.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function loadIFrame()
      {
          var iframe = document.createElement("iframe");
          iframe.src = "resources/fragment-frame.html#34";
          iframe.onload = frameLoaded;
          document.body.appendChild(iframe);
      }

      function frameLoaded()
      {
          console.log("Done.");
      }
  `);

  NetworkTestRunner.recordNetwork();
  ConsoleTestRunner.addConsoleSniffer(step2);
  TestRunner.evaluateInPage('loadIFrame()', function() {});


  function step2() {
    TestRunner.deprecatedRunAfterPendingDispatches(step3);
  }

  function step3() {
    var childFrame = TestRunner.resourceTreeModel.mainFrame.childFrames[0];
    TestRunner.addResult('Child frame url: ' + childFrame.url);
    var childFrameResource = childFrame.resources()[0];
    TestRunner.addResult('Child frame resource url: ' + childFrameResource.url);

    function filterFrame(request) {
      return request.url().indexOf('fragment-frame.html') !== -1;
    }

    var request = NetworkTestRunner.networkRequests().filter(filterFrame)[0];
    TestRunner.addResult('Child frame request url: ' + request.url());
    TestRunner.completeTest(step2);
  }
})();
