// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Test that completions in the context of an iframe with a different origin will result in
      names of its global variables. Test passes if all global variables are found among completions
      AND there are NO console messages. Bug 65457.
      https://bugs.webkit.org/show_bug.cgi?id=65457\n`);
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.addIframe("http://localhost:8000/devtools/resources/console-cd-completions-iframe.html", {
    name: "myIFrame"
  });
  await TestRunner.evaluateInPagePromise(`
      window.counter = 0;
      var handler = {
          get: function(target, name){
              window.counter++;
              return Reflect.get.apply(this, arguments);
          },
          set: function(target, name){
              window.counter++;
              return Reflect.set.apply(this, arguments);
          },
          getPrototypeOf: function(target) {
              window.counter++;
              return Reflect.getPrototypeOf.apply(this, arguments);
          },
          setPrototypeOf: function(target) {
              window.counter++;
              return Reflect.setPrototypeOf.apply(this, arguments);
          },
          isExtensible: function(target) {
              window.counter++;
              return Reflect.isExtensible.apply(this, arguments);
          },
          isExtensible: function(target) {
              window.counter++;
              return Reflect.isExtensible.apply(this, arguments);
          },
          isExtensible: function(target) {
              window.counter++;
              return Reflect.isExtensible.apply(this, arguments);
          },
          preventExtensions: function() {
              window.counter++;
              return Reflect.preventExtensions.apply(this, arguments);
          },
          getOwnPropertyDescriptor: function() {
              window.counter++;
              return Reflect.getOwnPropertyDescriptor.apply(this, arguments);
          },
          defineProperty: function() {
              window.counter++;
              return Reflect.defineProperty.apply(this, arguments);
          },
          has: function() {
              window.counter++;
              return Reflect.has.apply(this, arguments);
          },
          get: function() {
              window.counter++;
              return Reflect.get.apply(this, arguments);
          },
          set: function() {
              window.counter++;
              return Reflect.set.apply(this, arguments);
          },
          deleteProperty: function() {
              window.counter++;
              return Reflect.deleteProperty.apply(this, arguments);
          },
          ownKeys: function() {
              window.counter++;
              return Reflect.ownKeys.apply(this, arguments);
          },
          apply: function() {
              window.counter++;
              return Reflect.apply.apply(this, arguments);
          },
          construct: function() {
              window.counter++;
              return Reflect.construct.apply(this, arguments);
          }
      };
      window.proxy1 = new Proxy({ a : 1}, handler);
      window.proxy2 = new Proxy(window.proxy1, handler);


      var MyClass = function () {},
          mixin = function () {
              this.myMethod = console.log.bind(console, 'myMethod called');
          };
      MyClass.prototype = Object.create(null);
      mixin.call(MyClass.prototype);
      window.x = new MyClass();
  `);

  ConsoleTestRunner.changeExecutionContext('myIFrame');

  ObjectUI.javaScriptAutocomplete._completionsForExpression('', 'myGlob').then(checkCompletions.bind(this));
  function checkCompletions(completions) {
    TestRunner.addResult('myGlob completions:');
    dumpCompletions(completions, ['myGlobalVar', 'myGlobalFunction']);
    requestIFrameCompletions();
  }

  function requestIFrameCompletions() {
    ConsoleTestRunner.changeExecutionContext('top');
    ObjectUI.javaScriptAutocomplete._completionsForExpression('myIFrame.', '').then(checkIframeCompletions.bind(this));
  }

  function checkIframeCompletions(completions) {
    TestRunner.addResult('myIFrame completions:');
    dumpCompletions(completions, []);
    requestProxyCompletions();
  }


  function requestProxyCompletions() {
    ConsoleTestRunner.changeExecutionContext('top');
    ObjectUI.javaScriptAutocomplete._completionsForExpression('window.proxy2.', '')
        .then(checkProxyCompletions.bind(this));
  }

  function checkProxyCompletions(completions) {
    TestRunner.addResult('proxy completions:');
    dumpCompletions(completions, ['a']);
    TestRunner.evaluateInPage('window.counter', dumpCounter);
  }

  function dumpCounter(result) {
    TestRunner.addResult('window.counter = ' + result);
    requestMyClassWithMixinCompletions();
  }


  function requestMyClassWithMixinCompletions() {
    ConsoleTestRunner.changeExecutionContext('top');
    ObjectUI.javaScriptAutocomplete._completionsForExpression('window.x.', '')
        .then(checkMyClassWithMixinCompletions.bind(this));
  }

  function checkMyClassWithMixinCompletions(completions) {
    TestRunner.addResult('MyClass with mixin completions:');
    dumpCompletions(completions, ['myMethod']);
    requestObjectCompletions();
  }


  function requestObjectCompletions() {
    ConsoleTestRunner.changeExecutionContext('top');
    ObjectUI.javaScriptAutocomplete._completionsForExpression('Object.', '').then(checkObjectCompletions.bind(this));
  }

  async function checkObjectCompletions(completions) {
    TestRunner.addResult('Object completions:');
    dumpCompletions(completions, ['getOwnPropertyNames', 'getOwnPropertyDescriptor', 'keys']);
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }


  function dumpCompletions(completions, expected) {
    var completionSet = new Set(completions.map(c => c.text));
    var notFound = false;
    for (var completion of expected) {
      if (completionSet.has(completion)) {
        TestRunner.addResult(completion);
      } else {
        TestRunner.addResult('NOT FOUND: ' + completion);
        notFound = true;
      }
    }
    if (notFound)
      TestRunner.addResult(JSON.stringify(completions));
  }
})();
