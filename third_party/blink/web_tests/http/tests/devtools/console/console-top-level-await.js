// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult('Tests that evaluation with top-level await may be performed in console.');

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    function foo(x) {
      return x;
    }

    function koo() {
      return Promise.resolve(4);
    }
  `);

  var test = ConsoleTestRunner.evaluateInConsolePromise;
  await test('await Promise.resolve(1)');
  await test('{a:await Promise.resolve(1)}');
  await test('$_');
  await test('let {a,b} = await Promise.resolve({a: 1, b:2}), f = 5;');
  await test('a');
  await test('b');
  await test('let c = await Promise.resolve(2)');
  await test('c');
  await test('let d;');
  await test('d');
  await test('let [i,{abc:{k}}] = [0,{abc:{k:1}}];');
  await test('i');
  await test('k');
  await test('var l = await Promise.resolve(2);');
  await test('l');
  await test('foo(await koo());');
  await test('$_');
  await test('const m = foo(await koo());');
  await test('m');
  await test('const n = foo(await\nkoo());');
  await test('n');
  await test('`status: ${(await Promise.resolve({status:200})).status}`');
  await test('for (let i = 0; i < 2; ++i) await i');
  await test('for (let i = 0; i < 2; ++i) { await i }');
  await test('await 0');
  await test('await 0;function foo(){}');
  await test('foo');
  await test('class Foo{}; await 1;');
  await test('Foo');
  await test('await 0;function* gen(){}');
  await test('for (let j = 0; j < 5; ++j) { await j; }')
  await test('j');
  await test('gen');
  await test('await 5; return 42;');
  await test('let o = await 1, p');
  await test('p');
  await test('let q = 1, s = await 2');
  await test('s');
  await test('await {...{foo: 42}}');
  await new Promise(resolve => ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(resolve));
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
