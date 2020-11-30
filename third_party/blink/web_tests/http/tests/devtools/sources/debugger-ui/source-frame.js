// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that it's possible to set breakpoint in source frame, and that source frame displays breakpoints and console errors.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function addErrorToConsole()
      {
          console.error("test error message");
      }

      function methodForBreakpoint()
      {
          alert("Hello world");
      }
  `);
  await TestRunner.addScriptTag('source-frame.js');
  await TestRunner.addScriptTag('../resources/script.js');

  await UI.viewManager.showView('resources');
  SourcesTestRunner.runDebuggerTestSuite([
    function testConsoleMessage(next) {
      SourcesTestRunner.showScriptSource('source-frame.js', didShowScriptSource);

      var shownSourceFrame;
      function didShowScriptSource(sourceFrame) {
        TestRunner.addResult('Script source was shown.');
        shownSourceFrame = sourceFrame;
        TestRunner.addSniffer(Sources.UISourceCodeFrame.prototype, '_addMessageToSource', didAddMessage);
        TestRunner.addSniffer(Sources.UISourceCodeFrame.prototype, '_removeMessageFromSource', didRemoveMessage);
        TestRunner.evaluateInPage('addErrorToConsole()');
      }

      async function didAddMessage(message) {
        if (this !== shownSourceFrame)
          return;
        // Messages can contain live locations.
        await TestRunner.waitForPendingLiveLocationUpdates();
        TestRunner.addResult('Message added to source frame: ' + message.text());
        queueMicrotask(() => {
          Console.ConsoleView.clearConsole();
        });
      }

      function didRemoveMessage(message) {
        if (this !== shownSourceFrame)
          return;
        TestRunner.addResult('Message removed from source frame: ' + message.text());
        next();
      }
    },

    async function testShowResource(next) {
      await UI.viewManager.showView('network');
      TestRunner.addSniffer(SourceFrame.SourceFrame.prototype, 'show', didShowSourceFrame);

      TestRunner.resourceTreeModel.forAllResources(visit);
      function visit(resource) {
        if (resource.url.indexOf('script.js') !== -1) {
          UI.panels.resources._sidebar.showResource(resource, 1);
          return true;
        }
      }

      function didShowSourceFrame() {
        next();
      }
    }
  ]);
})();
