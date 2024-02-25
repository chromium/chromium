// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests various extreme usages of console.log()\n`);
  await TestRunner.showPanel('console');
  await TestRunner.loadHTML(`
      <p id="node">
      </p>
  `);
  await TestRunner.evaluateInPagePromise(`
      function overrideToString()
      {
          console.error("FAIL: side effects, should not be called.");
          return "FAIL";
      }

      [Object, Array, Number, Boolean, String, Uint32Array, Node, Element].forEach(function(type) {
        type.prototype.toString = overrideToString;
      });

      console.log('string');
      console.log(42);
      console.log(false);
      console.log(undefined);
      console.log(null);
      console.log(NaN);
      console.log(-Infinity);
      console.log(-0);
      console.log(new Number(42));
      console.log(new Number(-42.42e-12));
      console.log(new Boolean(true));
      console.log(new String('foo'));
      console.log({__proto__: null});
      console.log(window);

      // Test DOMWrapper object.
      var node = document.getElementById('node');
      node.toString = overrideToString;
      node.__proto__.toString = overrideToString;
      console.log(node);

      var obj = {foo: 1, bar: 2};
      var arr = [1, 2, 3];
      console.log(obj);
      console.log(arr);

      console.log(new Uint32Array([1, 2, 3]));

      arr.push(obj);
      console.log(arr);

      var overridden = {toString: overrideToString};
      console.log(overridden);

      arr.push(overridden);
      console.log(arr);

      // Test recursive arrays.
      var a1 = [[1, [[2], 3], [[[[4]]], 5]]];
      var a2 = [];
      a2[3] = null;
      a2[5] = NaN;
      a1.push(a2);
      a2.push(a1);
      var a3 = [[a1], [[a2]]];
      a3.push(a3);
      console.log(a3);

      // This used to timeout.
      var timeout = {
        toString: function() {
          while (true) {
          }
        }
      };
      console.log(timeout);

      arr.push(timeout);
      console.log(arr);

      // This used to crash out of memory.
      const maxArrayLength = 4294967295;
      for (var i = 100000; i < maxArrayLength; i *= 10) {
        arr[i] = i;
        console.log(arr);
      }
      arr[maxArrayLength - 1] = a3;
      console.log(arr);

      // Test array length limit.
      const arrayLengthLimit = 10000;
      var arr = [];
      for (var i = 0; i < arrayLengthLimit + 1; ++i)
        arr[i] = i;
      console.log(arr);

      // Test array stack depth limit.
      var arr = [];
      for (var i = 0; i < arrayLengthLimit; ++i)
        arr = [arr];
      console.log(arr);
  `);
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
