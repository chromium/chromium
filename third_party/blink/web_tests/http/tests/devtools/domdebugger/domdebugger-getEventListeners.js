// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests RemoteObject.eventListeners.\n`);
  await TestRunner.loadHTML(`
          <div id="with-handlers" onclick="return 42;"></div>
          <div id="without-handlers"></div>
        `);
  await TestRunner.evaluateInPagePromise(`
          function foo() {}
          function boo() {}
          window.addEventListener("scroll", foo, true);
          document.getElementById("with-handlers").addEventListener("click", boo, true);
          document.getElementById("with-handlers").addEventListener("mouseout", foo, false);
      `);

  var windowObject;
  var divWithHandlers;
  var divWithoutHandlers;
  function dumpListeners(listeners) {
    listeners.sort((a, b) => a.type().localeCompare(b.type()));
    for (var listener of listeners) {
      const sourceURL = listener.sourceURL();
      const sourceURLForOutput = sourceURL.substr(sourceURL.lastIndexOf('/') + 1);

      TestRunner.addResult('type: ' + listener.type());
      TestRunner.addResult('useCapture: ' + listener.useCapture());
      TestRunner.addResult(`location: ${listener.location().lineNumber}:${listener.location().columnNumber}`);
      TestRunner.addResult('handler: ' + listener.handler().description);
      TestRunner.addResult('sourceURL: ' + sourceURLForOutput);
      TestRunner.addResult('');
    }
  }

  TestRunner.runTestSuite([
    async function testSetUp(next) {
      var result = await TestRunner.RuntimeAgent.evaluate('window', 'get-event-listeners-test');

      windowObject = TestRunner.runtimeModel.createRemoteObject(result);
      result = await TestRunner.RuntimeAgent.evaluate(
          'document.getElementById("with-handlers")', 'get-event-listeners-test');

      divWithHandlers = TestRunner.runtimeModel.createRemoteObject(result);
      result = await TestRunner.RuntimeAgent.evaluate(
          'document.getElementById("without-handlers")', 'get-event-listeners-test');

      divWithoutHandlers = TestRunner.runtimeModel.createRemoteObject(result);
      next();
    },

    function testWindowEventListeners(next) {
      TestRunner.domDebuggerModel.eventListeners(windowObject).then(dumpListeners).then(next);
    },

    function testDivEventListeners(next) {
      TestRunner.domDebuggerModel.eventListeners(divWithHandlers).then(dumpListeners).then(next);
    },

    function testDivWithoutEventListeners(next) {
      TestRunner.domDebuggerModel.eventListeners(divWithoutHandlers).then(dumpListeners).then(next);
    }
  ]);
})();
