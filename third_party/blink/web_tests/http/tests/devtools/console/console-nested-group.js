// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that console.group/groupEnd messages won't be coalesced. Bug 56114. Bug 63521.\n`);

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    console.group("outer group");
    console.group("inner group");
    console.log("Message inside inner group");
    console.groupEnd();
    console.groupEnd();
    console.log("Message that must not be in any group");


    var groupCount = 3;
    for (var i = 0; i < groupCount; i++) {
      console.group("One of several groups which shouldn't be coalesced.");
    }
    console.log("Message inside third group");
    for (var i = 0; i < groupCount; i++) {
      console.groupEnd();
    }
  `);

  await ConsoleTestRunner.dumpConsoleMessagesWithClasses();
  TestRunner.completeTest();
})();
