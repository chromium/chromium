// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that content scripts are reported.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function createContentScript()
      {
          testRunner.evaluateScriptInIsolatedWorld(239, "42\\n//# sourceURL=239.js");
          testRunner.evaluateScriptInIsolatedWorld(42, "239\\n//# sourceURL=42.js");
      }
  `);

  TestRunner.evaluateInPage('createContentScript()', step1);
  function step1() {
    var scripts = SourcesTestRunner.queryScripts(function(script) {
      return script.isContentScript() && !script.isInternalScript;
    });
    TestRunner.addResult('Content Scripts:');
    for (var i = 0; i < scripts.length; ++i) {
      TestRunner.addResult('#' + (i + 1) + ':');
      TestRunner.addResult('  sourceURL:' + scripts[i].sourceURL);
      TestRunner.addResult('  endColumn:' + scripts[i].endColumn);
    }
    TestRunner.completeTest();
  }
})();
