// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console produces instant previews for arrays and objects.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
          console.log("Mutating object in a loop");
          var object = { a: 0, b: 0 };
          for (var i = 0; i < 3; ++i) {
              object.c = i;
              console.log(object);
          }

          console.log("Mutating array in a loop");
          var array = [0, 0, 0];
          for (var i = 0; i < 3; ++i) {
              array[2] = i;
              console.log(array);
          }

          console.log("Object with many properties");
          var objectWithManyProperties = {};
          for (var i = 0; i < 10; ++i) {
              objectWithManyProperties["property_" + i] = i;
          }
          console.log(objectWithManyProperties);

          console.log("Array with many properties");
          var arrayWithManyProperties = [0, 1];
          for (var i = 0; i < 10; ++i) {
              arrayWithManyProperties["property_" + i] = i;
          }
          console.log(arrayWithManyProperties);

          console.log("Array with gaps and overflow");
          var arrayWithGapsAndOverflow = [];
          for (var i = 0; i < 101; i++) {
              arrayWithGapsAndOverflow[57 * i + 32] = i;
          }
          console.log(arrayWithGapsAndOverflow);

          console.log("Array with gaps without overflow");
          var arrayWithGapsWithoutOverflow = [];
          for (var i = 0; i < 99; i++) {
              arrayWithGapsWithoutOverflow[57 * i + 32] = i;
          }
          console.log(arrayWithGapsWithoutOverflow);

          console.log("Object with proto");
          var objectWithProto = { d: 1 };
          objectWithProto.__proto__ = object;
          console.log(objectWithProto);

          console.log("Sparse array");
          var sparseArray = new Array(150);
          sparseArray[50] = 50;
          console.log(sparseArray);

          console.log("Dense array with indexes and propeties");
          var denseArray = new Array(150);
          for (var i = 0; i < 150; ++i) {
              denseArray[i] = i;
              denseArray["property_" + i] = i;
          }
          console.log(denseArray);

          console.log("Object with properties containing whitespaces");
          var obj = {};
          obj[" a b "] = " a b ";
          obj["c d"] = "c d";
          obj[""] = "";
          obj["  "] = "  ";
          obj["a\\n\\nb\\nc"] = "a\\n\\nb\\nc";
          console.log(obj);

          console.log("Object with a document.all property");
          console.log({all: document.all});

          console.log("Object with special numbers");
          var obj = { nan: NaN, posInf: Infinity, negInf: -Infinity, negZero: -0 };
          console.log(obj);

          console.log("Object with exactly 5 properties: expected to be lossless");
          console.log({a:1, b:2, c:3, d:4, e:5});

          console.log({null:null, undef:undefined, regexp: \/^[regexp]$\/g, bool: false});

          class IHavePrivateProperties {
              #privateProperty1 = 1;
              #privateProperty2 = 2;
              regularProperty = 3;
          }
          console.log(new IHavePrivateProperties)
  `);

  ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.addResult('Expanded all messages');
  ConsoleTestRunner.expandConsoleMessages(step3);

  function step3() {
    ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
