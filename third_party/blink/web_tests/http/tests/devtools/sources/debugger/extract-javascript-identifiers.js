// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the extraction of javascript identifier names from function text.\n`);
  await TestRunner.showPanel('sources');

  TestRunner.runTestSuite([
    function testFunctionArguments(next) {
      extract('function foo(a, b) { }', next);
    },

    function testSimpleVariable(next) {
      extract('function foo() { var a = 1; }', next);
    },

    function testMemberExpression(next) {
      extract('function foo() { var a = b.c.d.e; }', next);
    },

    function testFunctionCall(next) {
      extract('function foo() { var a = doSomething(b, true, 10); }', next);
    },

    function testPropertyLiteral(next) {
      extract('function foo() { var a = b[\'test\'];}', next);
    },

    function testComputedProperty(next) {
      extract('function foo() { var a = b[variableName];}', next);
    },

    function testNestedFunction1(next) {
      extract('function foo() { var a = 1; function bar() { var b = 1; } var c = 3;}', next);
    },

    function testNestedFunction2(next) {
      extract('function foo() { var a = 1; var bar = function (){ var b = 1; }; var c = 3;}', next);
    },

    function testNestedFunction3(next) {
      extract('function foo() { var a = x => x * 2 }', next);
    },
  ]);

  function extract(text, next) {
    TestRunner.addResult('Text:');
    TestRunner.addResult('    ' + text + '\n');
    Formatter.formatterWorkerPool().javaScriptIdentifiers(text).then(onIdentifiers).then(next);
  }

  function onIdentifiers(ids) {
    TestRunner.addResult('Identifiers:');
    for (var id of ids)
      TestRunner.addResult(`    id: ${id.name}    offset: ${id.offset}`);
  }
})();
