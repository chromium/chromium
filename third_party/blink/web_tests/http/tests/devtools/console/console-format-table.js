// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests console.table.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      console.table();
      console.table(null);

      console.log("Array of arrays");
      console.table([[1,2,3], [4,5,6]]);

      console.log("Large array of arrays");
      console.table([[1,2,3], [4,5,6], [1,2,3], [4,5,6], [1,2,3], [4,5,6], [1,2,3], [4,5,6]]);

      console.log("Array or array and object");
      console.table([[1,2,3], {a:1, b:2, c:3}]);

      console.log("Object table");
      console.table({"foo": {a:1, b:2}, "bar": {a:3, b:4}});

      console.log("Null as columns");
      console.table([[1,2,3], [4,5,6]], null);

      console.log("Digit as columns");
      console.table([[1,2,3], [4,5,6]], 0);

      console.log("String as columns");
      console.table([[1,2,3], [4,5,6]], "0");

      console.log("Random string as columns");
      console.table([[1,2,3], [4,5,6]], "Foo");

      console.log("Array of strings as columns");
      console.table([{a:1, b:2, c:3}, {a:"foo", b:"bar"}], ["a", "b"]);

      console.log("Good and bad column names");
      console.table([{a:1, b:2, c:3}, {a:"foo"}], ["a", "b", "d"]);

      console.log("Missing column name");
      console.table([{a:1, b:2, c:3}, {a:"foo"}], ["d"]);

      console.log("Shallow array");
      console.table([1, "foo", null]);

      console.log("Shallow array with 'Value' column");
      console.table([1, {Value: 2}]);

      console.log("Deep and shallow array");
      console.table([1, "foo", [2]]);

      console.log("Non-standard call should use fallback");
      console.table("foo", [1,2,3]);
  `);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
