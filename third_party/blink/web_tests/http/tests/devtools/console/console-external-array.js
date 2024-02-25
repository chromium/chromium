// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult('Tests that console logging detects external arrays as arrays.\n');

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    function logToConsole()
    {
        console.log(new Int8Array(10));
        console.log(new Int16Array(10));
        console.log(new Int32Array(10));
        console.log(new Uint8Array(10));
        console.log(new Uint16Array(10));
        console.log(new Uint32Array(10));
        console.log(new Float32Array(10));
        console.log(new Float64Array(10));

        console.dir(new Int8Array(10));
        console.dir(new Int16Array(10));
        console.dir(new Int32Array(10));
        console.dir(new Uint8Array(10));
        console.dir(new Uint16Array(10));
        console.dir(new Uint32Array(10));
        console.dir(new Float32Array(10));
        console.dir(new Float64Array(10));
    }
  `);

  TestRunner.evaluateInPage('logToConsole()', onLoggedToConsole);

  function onLoggedToConsole() {
    ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(onRemoteObjectsLoaded);
  }

  async function onRemoteObjectsLoaded() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
