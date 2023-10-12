// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests that console produces instant previews for arrays and objects.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      const objWithGetter = {a: 1, __proto__: 2};
      Object.defineProperty(objWithGetter, "foo", {enumerable: true, get: function() { return {a:1,b:2}; }});
      Object.defineProperty(objWithGetter, "bar", {enumerable: false, set: function(x) { this.baz = x; }});

      const arrayWithGetter = [1];
      arrayWithGetter[3] = 2;
      Object.defineProperty(arrayWithGetter, 4, {enumerable: true, get: function() { return 1; }});
      Object.defineProperty(arrayWithGetter, 5, {enumerable: false, set: function(x) { this.baz = x; }});

      const tests = [
        new Error('custom error with link www.chromium.org'),
        arrayWithGetter,
        objWithGetter,
        {str: "", nan: NaN, posInf: Infinity, negInf: -Infinity, negZero: -0},
        {null: null, undef: undefined, re: /^[regexp]$/g, constructedRe: new RegExp('foo/bar'), bool: false},
        new Proxy({a: 1}, {}),
        document.all,
      ];

      for (const test of tests)
        console.log(test);

      // Arrays can preview at most 100 items.
      for (let i = 0; i < tests.length; i += 100)
        console.log(tests.slice(i, i + 100));
  `);

  ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(onRemoteObjectsLoaded);
  async function onRemoteObjectsLoaded() {
    await ConsoleTestRunner.dumpConsoleMessages(false, true /* dumpClassNames */);
    TestRunner.completeTest();
  }
})();
