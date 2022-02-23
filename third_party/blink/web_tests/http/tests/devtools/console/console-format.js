// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console logging dumps proper messages.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.loadHTML(`
      <div id="x"></div>
      <p id="p"></p>

      <svg id="svg-node"/>
    `);
  await TestRunner.evaluateInPagePromise(`
      // Global Values
      var globals = [];

      function log(current)
      {
          console.log(globals[current]);
          console.log([globals[current]]);
      }

      var foo = { foo: "foo"};
      var bar = { bar: "bar" };
      bar.__proto__ = foo;
      var singleArray = ["test"];
      var array = ["test", "test2"];array.length = 10;
      array.foo = {};
      array[4] = "test4";

      var svg = document.getElementById("svg-node");
      console.log(array);
      console.log("%o", array);
      console.log("%O", array);
      console.log("Test for zero \\"%f\\" in formatter", 0);
      console.log("%% self-escape1", "dummy");
      console.log("%%s self-escape2", "dummy");
      console.log("%%ss self-escape3", "dummy");
      console.log("%%s%s%%s self-escape4", "dummy");
      console.log("%%%%% self-escape5", "dummy");
      console.log("%%%s self-escape6", "dummy");

      // Populate Globals
      var regex1 = /^url\\(\\s*(?:(?:"(?:[^\\\\\\"]|(?:\\\\[\\da-f]{1,6}\\s?|\\.))*"|'(?:[^\\\\\\']|(?:\\\\[\\da-f]{1,6}\\s?|\\.))*')|(?:[!#$%&*-~\\w]|(?:\\\\[\\da-f]{1,6}\\s?|\\.))*)\\s*\\)/i;
      var regex2 = new RegExp("foo\\\\\\\\bar\\\\sbaz", "i");
      var str = "test";
      var str2 = "test named \\"test\\"";
      var error = new Error;
      var errorWithMessage = new Error("my error message");
      var errorWithMultilineMessage = new Error("my multiline\\nerror message");
      var node = document.getElementById("p");
      var func = function() { return 1; };
      var multilinefunc = function() {
          return 2;
      };
      var num = 1.2e-1;
      var linkify = "http://webkit.org/";
      var valuelessAttribute = document.createAttribute("attr");
      var valuedAttribute = document.createAttribute("attr");
      valuedAttribute.value = "value";
      var existingAttribute = document.getElementById("x").attributes[0];
      var throwingLengthGetter = {get length() { throw "Length called"; }};
      var objectWithNonEnumerables = Object.create({ foo: 1 }, {
          __underscoreNonEnumerableProp: { value: 2, enumerable: false },
          abc: { value: 3, enumerable: false },
          getFoo: { value: function() { return this.foo; } },
          bar: { get: function() { return this.bar; }, set: function(x) { this.bar = x; } }
      });
      objectWithNonEnumerables.enumerableProp = 4;
      objectWithNonEnumerables.__underscoreEnumerableProp__ = 5;
      var negZero = 1 / Number.NEGATIVE_INFINITY;
      var textNode = document.getElementById("x").nextSibling;
      var arrayLikeFunction = function( /**/ foo/**/, /*/**/bar,
      /**/baz) {};
      arrayLikeFunction.splice = function() {};
      var tinyTypedArray = new Uint8Array([3]);
      var smallTypedArray = new Uint8Array(new ArrayBuffer(400));
      smallTypedArray["foo"] = "bar";
      var bigTypedArray = new Uint8Array(new ArrayBuffer(400 * 1000 * 1000));
      bigTypedArray.PASS = "Non-element properties should be displayed.";
      var namespace = {};
      namespace.longSubNamespace = {};
      namespace.longSubNamespace.x = {};
      namespace.longSubNamespace.x.className = function(){};
      var instanceWithLongClassName = new namespace.longSubNamespace.x.className();
      var bigArray = [];
      bigArray.length = 200;
      bigArray.fill(1);
      var boxedNumberWithProps = new Number(42);
      boxedNumberWithProps[1] = "foo";
      boxedNumberWithProps["a"] = "bar";
      var boxedStringWithProps = new String("abc");
      boxedStringWithProps["01"] = "foo";
      boxedStringWithProps[3] = "foo";
      boxedStringWithProps["a"] = "bar";

      globals = [
          regex1, regex2, str, str2, error, errorWithMessage, errorWithMultilineMessage, node, func, multilinefunc,
          num, linkify, null, undefined, valuelessAttribute, valuedAttribute, existingAttribute, throwingLengthGetter,
          NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, array, {}, [function() {}], bar, svg,
          objectWithNonEnumerables, negZero, Object.create(null), Object, Object.prototype, arrayLikeFunction,
          new Number(42), new String("abc"), new Uint16Array([1, 2, 3]), textNode, domException(),
          tinyTypedArray, smallTypedArray, bigTypedArray, instanceWithLongClassName, bigArray, singleArray,
          boxedNumberWithProps, boxedStringWithProps
      ];
      function domException()
      {
          var result = "FAIL";
          try {
              var a = document.createElement("div");
              var b = document.createElement("div");
              a.removeChild(b);
          } catch(e) {
              e.stack = "";
              result = e;
          }
          return result;
      }
      //# sourceURL=console-format.js
    `);

  TestRunner.hideInspectorView();
  TestRunner.evaluateInPage('globals.length', loopOverGlobals.bind(this, 0));

  function loopOverGlobals(current, total) {
    function advance() {
      var next = current + 1;
      if (next == total)
        ConsoleTestRunner.waitForRemoteObjectsConsoleMessages(onRemoteObjectsLoaded);
      else
        loopOverGlobals(next, total);
    }

    async function onRemoteObjectsLoaded() {
      await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
      TestRunner.addResult('Expanded all messages');
      ConsoleTestRunner.expandConsoleMessages(
          ConsoleTestRunner.expandConsoleMessagesErrorParameters.bind(this, finish), undefined, function(section) {
            return section.element.firstChild.textContent !== '#text';
          });
    }

    async function finish() {
      await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
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
