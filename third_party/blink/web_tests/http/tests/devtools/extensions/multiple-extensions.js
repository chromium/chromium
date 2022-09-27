// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests co-existence of multiple DevTools extensions\n`);
  await TestRunner.loadTestModule('extensions_test_runner');

  const tests = [
    function extension_testCreatePanel(nextTest) {
      function onPanelCreated(panel) {
        output("Panel created");
        dumpObject(panel);
        evaluateOnFrontend("TestRunner.extensionTestComplete();");
      }
      var basePath = location.pathname.replace(/\/[^/]*$/, "/");
      webInspector.panels.create("Test Panel", basePath + "extension-panel.png", basePath + "extension-panel.html", onPanelCreated);
    },
  ];
  var pendingTestCount = 2;
  TestRunner.extensionTestComplete = function() {
    if (--pendingTestCount)
      ExtensionsTestRunner.runExtensionTests(tests);
    else
      TestRunner.completeTest();
  }
  await ExtensionsTestRunner.runExtensionTests(tests);
})();
