// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';

(async function() {
  TestRunner.addResult(`Tests ignoreCache flag of WebInspector.inspectedPage.reload()\n`);
  await TestRunner.navigatePromise('resources/random-script.html');
  await ExtensionsTestRunner.runExtensionTests([
    function extension_testIgnoreCache(nextTest) {
      var beforeReload;
      var afterReloadWithIgnoreCache;
      var afterNormalReload;

      function onNormalReload() {
        webInspector.inspectedWindow.eval("randomValue", function(value) {
          afterNormalReload = value;
          evaluateOnFrontend("TestRunner.waitForPageLoad(reply)", onReloadWithIgnoreCache);
          webInspector.inspectedWindow.reload({ ignoreCache: true });
        });
      };

      function onReloadWithIgnoreCache() {
        webInspector.inspectedWindow.eval("randomValue", function(value) {
          afterReloadWithIgnoreCache = value;
          output("afterNormalReload " + (afterNormalReload === beforeReload ? "===" : "!==" ) + " beforeReload");
          output("afterNormalReload " + (afterNormalReload === afterReloadWithIgnoreCache ? "===" : "!==" ) + " afterReloadWithIgnoreCache");
          nextTest();
        });
      }

      webInspector.inspectedWindow.eval("randomValue", function(value) {
        beforeReload = value;
        evaluateOnFrontend("TestRunner.waitForPageLoad(reply)", onNormalReload);
        webInspector.inspectedWindow.reload();
      });
    }
  ]);
})();
