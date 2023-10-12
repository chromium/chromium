// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult('Tests that console logging dumps proper messages.\n');

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    (function onload()
    {
        var obj = new Object();
        obj["a"] = 1;
        obj[Symbol()] = 2;
        obj[Symbol("a")] = 3;
        obj[Symbol("a")] = Symbol.iterator;
        obj[Symbol.iterator] = Symbol("foo");
        console.dir(obj);

        // This used to crash in debug build.
        console.dir(Symbol());

        [new Map(), new WeakMap()].forEach(function(m) {
            m.set(obj, {foo: 1});
            console.dir(m);
        });
        [new Set(), new WeakSet()].forEach(function(s) {
            s.add(obj);
            console.dir(s);
        });

        // Test circular dependency by entries.
        var s1 = new Set();
        var s2 = new Set();
        s1.add(s2);
        s2.add(s1);
        console.dir(s1);

        // Test "No Entries" placeholder.
        console.dir(new WeakMap());

        // Test Map/Set iterators.
        var m = new Map();
        m.set(obj, {foo: 1});
        var s = new Set();
        s.add(obj);
        [m, s].forEach(function(c) {
            console.dir(c.keys());
            console.dir(c.values());
            console.dir(c.entries());
        });

        class FooClass {
            jump(x) { return 1 }
            badArrow(x = a => 2) { return "looooooooooooooooooooooooooooooooooooooooooooooooooooong" }
        }
        var fooInstance = new FooClass();

        var fns = [
            class{method(){ return 1 }},
            class        classWithWhitespace     {       method(){ return 1 }     },
            FooClass,
            fooInstance.jump,
            class BarClass extends FooClass{},
            class BarClass2 extends class base{} {},
            class BarClass3 extends function base2(a, b) {} {},
            _ => { return 1 },
            (x) => { return 1 },
            (x, y, z) => { return 1 },
            ({}) => { return 1 },
            ([]) => { return 1 },
            () => { return "short" },
            () => { return "looooooooooooooooooooooooooooooooooooooooooooooooooooong" },
            (...x) => { return 1 },
            (x, y, ...z) => { return 1 },
            function (...x) { return 1 },
            function (x, y, ...z) { return 1 },
            function({a}){ return 1 },
            function([a]){ return 1 },
            function({a, b}){ return 1 },
            function(...{a}){ return 1 },
            function(a = (1), b){ return 1 },
            function(a = {x: (1)}, b){ return 1 },
            function(a = (x) => { return 1 }, b){ return 2 },
            function({a: b}){ return 1 },
            function(c = ")", {a: b}){ return 1 }
        ];
        console.dir(fns);

        var badFns = [
            fooInstance.badArrow,
            function(a = ") {", b){ return 1 },
            function(a = function(){ return 1 }, b){ return 2 },
            function(a = class{ constructor(){} }){ return 2 }
        ];
        console.dir(badFns);

    })();
  `);

  ConsoleTestRunner.expandConsoleMessages(dumpConsoleMessages);

  async function dumpConsoleMessages() {
    await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
    TestRunner.completeTest();
  }
})();
