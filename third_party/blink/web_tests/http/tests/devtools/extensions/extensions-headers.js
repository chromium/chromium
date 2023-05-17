// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';

(async function() {
  TestRunner.addResult(`Tests WebInspector extension API\n`);
  await TestRunner.loadHTML(`
    <div style="white-space: pre" id="headers"></div>
    <script>
      function doXHR()
      {
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "../resources/echo-headers.php", false);
          xhr.send(null);
          return xhr.responseText;
      }
    </script>
  `);
  await ExtensionsTestRunner.runExtensionTests([
    function extension_testAddHeaders(nextTest) {
      webInspector.network.addRequestHeaders({
        "x-webinspector-extension": "test",
        "user-agent": "Mozilla/4.0 (compatible; MSIE 7.0b; Windows NT 6.0)"
      });
      function cleanUpHeaders(headers) {
        output(headers);
        webInspector.network.addRequestHeaders({
          "x-webinspector-extension": null,
          "user-agent": null
        });
      }
      webInspector.inspectedWindow.eval("doXHR()", callbackAndNextTest(cleanUpHeaders, nextTest));
    }
  ]);
})();
