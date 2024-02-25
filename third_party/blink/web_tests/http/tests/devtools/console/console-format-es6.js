// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult('Tests that console properly displays information about ES6 features.\n');

  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    var globals = [];
    function log(current)
    {
        console.log(globals[current]);
        console.log([globals[current]]);
    }
    (function onload()
    {
        var p = Promise.reject(-0);
        p.catch(function() {});

        var p2 = Promise.resolve(1);
        var p3 = new Promise(() => {});

        var smb1 = Symbol();
        var smb2 = Symbol("a");
        var obj = {
            get getter() {}
        };
        obj["a"] = smb1;
        obj[smb2] = 2;

        var map = new Map();
        var weakMap = new WeakMap();
        map.set(obj, {foo: 1});
        weakMap.set(obj, {foo: 1});

        var set = new Set();
        var weakSet = new WeakSet();
        set.add(obj);
        weakSet.add(obj);

        var mapMap0 = new Map();
        mapMap0.set(new Map(), new WeakMap());
        var mapMap = new Map();
        mapMap.set(map, weakMap);

        var setSet0 = new Set();
        setSet0.add(new WeakSet());
        var setSet = new Set();
        setSet.add(weakSet);

        var bigmap = new Map();
        bigmap.set(" from str ", " to str ");
        bigmap.set(undefined, undefined);
        bigmap.set(null, null);
        bigmap.set(42, 42);
        bigmap.set({foo:"from"}, {foo:"to"});
        bigmap.set(["from"], ["to"]);

        var genFunction = function *() {
            yield 1;
            yield 2;
        }
        var generator = genFunction();

        globals = [
            p, p2, p3, smb1, smb2, obj, map, weakMap, set, weakSet,
            mapMap0, mapMap, setSet0, setSet, bigmap, generator
        ];

    })();
  `);

  TestRunner.evaluateInPage('globals.length', loopOverGlobals.bind(this, 0));

  function loopOverGlobals(current, total) {
    function advance() {
      var next = current + 1;

      if (next == total)
        finish();
      else
        loopOverGlobals(next, total);
    }

    async function finish() {
      await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
      TestRunner.addResult('Expanded all messages');
      ConsoleTestRunner.expandConsoleMessages(dumpConsoleMessages);
    }

    async function dumpConsoleMessages() {
      await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
      TestRunner.completeTest();
    }

    TestRunner.evaluateInPage('log(' + current + ')');
    TestRunner.deprecatedRunAfterPendingDispatches(evalInConsole);

    function evalInConsole() {
      ConsoleTestRunner.evaluateInConsole('globals[' + current + ']');
      TestRunner.deprecatedRunAfterPendingDispatches(advance);
    }
  }
})();
