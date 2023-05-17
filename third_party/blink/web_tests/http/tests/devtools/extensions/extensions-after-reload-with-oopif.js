// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that extension does not get disconnected after a page with OOPIFs is reloaded\n`);
  await TestRunner.navigatePromise('http://localhost:8000/devtools/extensions/resources/page-with-oopif.html');

  await ExtensionsTestRunner.runExtensionTests([
    function extension_testRequestNotification(nextTest) {
      let count = 0;
      function onRequestFinished(request) {
        if (!request.request.url.endsWith("abe.png"))
          return;
        output("Request finished: " + request.request.url.replace(/.*((\/[^/]*){3}$)/,"...$1"));
        if (++count === 2) {
          nextTest();
          return;
        }
        webInspector.inspectedWindow.reload();
      }
      webInspector.network.onRequestFinished.addListener(onRequestFinished);
      webInspector.inspectedWindow.reload();
    }
  ]);
})();
