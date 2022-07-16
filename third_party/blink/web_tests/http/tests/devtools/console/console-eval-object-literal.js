// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that evaluating object literal in the console correctly reported.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');


  var commands = [
    '{a:1, b:2}', '{a:1}', '{var a = 1; eval("{ a:1, b:2 }");}', '{ for (var i = 0; i < 5; ++i); }',
    '{ a: 4 }),({ b: 7 }', '{ let a = 4; a; }', '{ let a = 4; }; { let b = 5; };', '{ a: 4 } + { a: 5 }',
    '{ a: 4 }, { a: 5 }', 'var foo = 4;',

    // Test that detection doesn't incur side effects.
    '{ a: foo++ }', 'foo;'
  ];

  var current = -1;
  loopOverCommands();

  async function loopOverCommands() {
    ++current;

    if (current < commands.length) {
      ConsoleTestRunner.evaluateInConsole(commands[current], loopOverCommands);
    } else {
      await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
      TestRunner.completeTest();
    }
  }
})();
