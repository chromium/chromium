// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test checks text editor javascript formatting.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
<pre id="codeSnippet">/**
* Multi-line comment
*
*/
function foo(n) {
// one-line comment
function bar() {
return 42;D

var sum = 0;
for (var i = 0; i &lt; n; ++i) {
for (var j = 0; j &lt; n; ++j) {
sum += i + j;DD


if (sum &gt; 1000) {
while (sum &gt; 0) {
--sum;DDD
</pre>
  `);
  await TestRunner.dumpInspectedPageElementText('#codeSnippet');
  await TestRunner.evaluateInPagePromise(`
      function codeSnippet() {
          return document.getElementById("codeSnippet").textContent;
      }
  `);

  var textEditor = SourcesTestRunner.createTestEditor();
  textEditor.setMimeType('text/javascript');
  textEditor.setReadOnly(false);
  textEditor.element.focus();

  function step2(result) {
    var codeLines = result;
    SourcesTestRunner.typeIn(textEditor, codeLines, step3);
  }

  function step3() {
    TestRunner.addResult('============ editor contents start ============');
    TestRunner.addResult(textEditor.text().replace(/ /g, '.'));
    TestRunner.addResult('============ editor contents end ============');
    TestRunner.completeTest();
  }

  TestRunner.evaluateInPage('codeSnippet();', step2);
})();
