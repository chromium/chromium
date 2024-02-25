// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult('Tests that command line api works.\n');

  await TestRunner.showPanel('console');
  await TestRunner.loadHTML(`
    <p id='foo'>
      Tests that command line api works.
    </p><p id='bar'></p>
  `);

  await TestRunner.evaluateInPagePromise(`
    function Foo() {}
    foo = new Foo();
  `);

  var expressions = [
    'String($0)',
    '$3',
    'String(keys([3,4]))',
    'String(values([3,4]))',
    `String($('#foo'))`,
    `String($('#foo', document.body))`,
    `String($('#foo', 'non-node'))`,
    `String($('#foo', $('#bar')))`,
    `String($$('p'))`,
    `String($$('p', document.body))`,
    `String($('foo'))`,
    `console.assert(keys(window).indexOf('__commandLineAPI') === -1)`,
    'queryObjects(Foo)'
  ];

  ElementsTestRunner.selectNodeWithId('foo', step1);

  function step1(node) {
    var expression = expressions.shift();
    if (!expression) {
      step2();
      return;
    }
    Common.Console.Console.instance().log('');
    ConsoleTestRunner.evaluateInConsole(expression, step1);
  }

  async function step2() {
    await TestRunner.evaluateInPagePromise(`
      (function assertNoBoundCommandLineAPI() {
        ['__commandLineAPI', '__scopeChainForEval'].forEach(function(name) {
          console.assert(!(name in window), 'FAIL: Should be no ' + name);
        });
      })();
    `);
    step3();
  }

  async function step3() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
