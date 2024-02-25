// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as ProtocolClient from 'devtools/core/protocol_client/protocol_client.js';

(async function() {
  TestRunner.addResult(
      `Tests that if iframe is loaded and then deleted, inspector could still show its content. Note that if iframe.src is changed to "javascript:'...some html...'" after loading, then we have different codepath, hence two tests;\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      var iframe;

      function loadIframe()
      {
          iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
          iframe.onload = iframeLoaded;
          iframe.src = "resources/resource.php";
      }

      function iframeLoaded()
      {
          document.body.removeChild(iframe);

          loadIframeAndChangeSrcAfterLoad();
      }

      function loadIframeAndChangeSrcAfterLoad()
      {
          iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
          iframe.onload = iframeLoadedChangeSrc;
          iframe.src = "resources/resource.php";
      }

      function iframeLoadedChangeSrc()
      {
          iframe.onload = null;
          iframe.src = "javascript:'<html></html>'";
          document.body.removeChild(iframe);
          console.log("Done.");
      }
  `);

  ProtocolClient.InspectorBackend.test.suppressRequestErrors = true;
  NetworkTestRunner.recordNetwork();
  ConsoleTestRunner.addConsoleSniffer(step2);
  TestRunner.evaluateInPage('loadIframe()');

  async function step2() {
    var requests = NetworkTestRunner.networkRequests();
    var request1 = requests[requests.length - 2];
    TestRunner.addResult(request1.url());
    TestRunner.addResult('resource.type: ' + request1.resourceType());
    var { content, error, isEncoded } = await request1.requestContent();
    TestRunner.addResult('resource.content after requesting content: ' + content);

    var request2 = requests[requests.length - 1];
    TestRunner.addResult(request2.url());
    TestRunner.addResult('resource.type: ' + request2.resourceType());
    var { content, error, isEncoded } = await request2.requestContent();
    TestRunner.addResult('resource.content after requesting content: ' + content);

    TestRunner.completeTest();
  }
})();
