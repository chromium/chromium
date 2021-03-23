// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`<script>function foo() {
          console.log(42);
      }
      //# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiYS5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbImEudHMiXSwibmFtZXMiOlsiZm9vIl0sIm1hcHBpbmdzIjoiQUFBQTtJQUNFQSxPQUFPQSxDQUFDQSxHQUFHQSxDQUFDQSxFQUFFQSxDQUFDQSxDQUFDQTtBQUNsQkEsQ0FBQ0EiLCJzb3VyY2VzQ29udGVudCI6WyJmdW5jdGlvbiBmb28oKSB7XG4gIGNvbnNvbGUubG9nKDQyKTtcbn1cbiJdfQ==
    </script>`);

  TestRunner.addResult(`Tests inline source with source map.`);
  await SourcesTestRunner.startDebuggerTestPromise();

  TestRunner.addResult('Set breakpoint at console.log line');
  let sourceFrame = await new Promise(
      resolve => SourcesTestRunner.showScriptSource('a.ts', resolve));
  SourcesTestRunner.toggleBreakpoint(sourceFrame, 1, false);
  await SourcesTestRunner.waitDebuggerPluginBreakpoints(sourceFrame);

  TestRunner.addResult('Call function and dump stack trace');
  TestRunner.evaluateInPageAnonymously('foo()');
  let callFrames = await SourcesTestRunner.waitUntilPausedPromise();
  await SourcesTestRunner.captureStackTrace(callFrames);

  TestRunner.addResult('Dump console mesage with its location:');
  let messagePromise = ConsoleTestRunner.waitUntilMessageReceivedPromise();
  SourcesTestRunner.resumeExecution();
  await messagePromise;
  await ConsoleTestRunner.dumpConsoleMessages();

  SourcesTestRunner.completeDebuggerTest();
})();
