// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests installing compiler source map in scripts panel.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.addScriptTag('resources/compiled-2.js');

  await TestRunner.evaluateInPagePromise(`addElements()`);
  await TestRunner.evaluateInPagePromise(`
      function clickButton()
      {
          document.getElementById('test').click();
      }

      function installScriptWithPoorSourceMap()
      {
          var script = document.createElement("script");
          script.setAttribute("src", "resources/compiled-with-wrong-source-map-url.js");
          document.head.appendChild(script);
      }
  `);

  SourcesTestRunner.runDebuggerTestSuite([function testSetBreakpoint(next) {
    SourcesTestRunner.showScriptSource('add-elements.js', didShowSource);

    async function didShowSource(sourceFrame) {
      TestRunner.addResult('Script source was shown.');
      await SourcesTestRunner.setBreakpoint(sourceFrame, 14, '', true);
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPage('setTimeout(clickButton, 0)');
    }

    async function paused(callFrames) {
      await SourcesTestRunner.captureStackTrace(callFrames);
      SourcesTestRunner.resumeExecution(next);
    }
  }]);
})();
