// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test checks text editor indent autodetection functionality\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
<div>--------------TEST 1--------------</div>
<pre id="test1" class="test">function foo() {
        return 42;
}
</pre>
<div>--------------TEST 2--------------</div>
<pre id="test2" class="test">console.log(&quot;Hello!&quot;);</pre>
<div>--------------TEST 3--------------</div>
<pre id="test3" class="test">/**
 * This is a header comment that spans
 * for a lot of lines
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 */
function foo() {
  return 42;
}
</pre>
<div>--------------TEST 4--------------</div>
<pre id="test4" class="test">function MyClass()
{
    this._foo = &quot;bar&quot;;
}

MyClass.prototype = {
    method1: function()
    {
        var sum = 0;
        for(var i = 0; i &lt; 100; ++i) {
            sum += i;
        }
        return sum;
    },

    method2: function()
    {
        while(true) {
            break;
        }
    },
}
</pre>
<div>--------------TEST 5--------------</div>
<pre id="test5" class="test">  a
  a
    b
    b
    b
c
c
</pre>
<div>--------------TEST 6--------------</div>
<pre id="test6" class="test">	tab
		tab
	tab
	tab
</pre>
<div>--------------TEST 7-------------- (empty content)</div>
<pre id="test7" class="test"></pre>
<div>--------------TEST 8--------------</div>
<pre id="test8" class="test">function foo() {
  var i = 0;
  function bar() {
    var a = [];
    a.push(1);
    a.push(12);
    a.push(42);
    a.push(44);
    return a.join(&quot;!&quot;);
  }

  (function() {
    var a = {
      a: function() {
        vbr b = [];
        b.push(1);
        b.push(12);
        b.push(42);
        b.push(44);
        b.push(44 * 2);
        return b.join(&quot;?&quot;);
      }
    };
  })();
}
</pre>
  `);
  await TestRunner.dumpInspectedPageElementText('body');
  await TestRunner.evaluateInPagePromise(`
      function codeSnippet(name) {
          return document.getElementById(name).textContent;
      }

      function codeSnippetsNumber() {
          return document.getElementsByClassName("test").length;
      }
  `);

  var textEditor = SourcesTestRunner.createTestEditor();
  textEditor.setMimeType('text/javascript');
  textEditor.setReadOnly(false);
  textEditor.element.focus();
  Common.settingForTest('textEditorAutoDetectIndent').set(true);
  function genericTest(snippetName, next) {
    var command = 'codeSnippet(\'' + snippetName + '\');';
    TestRunner.evaluateInPage(command, step2);
    function step2(result) {
      textEditor.setText(result);
      var indent = textEditor.indent();
      var description = indent === TextUtils.TextUtils.Indent.TabCharacter ? 'Tab' : indent.length + ' spaces';
      TestRunner.addResult('Autodetected indentation for ' + snippetName + ': ' + description);
      next();
    }
  }

  function onTestNumberReceived(result) {
    var testSuite = [];
    TestRunner.addResult('Tests number: ' + result);
    for (var i = 1; i <= result; ++i)
      testSuite.push(genericTest.bind(this, 'test' + i));

    TestRunner.runTestSuite(testSuite);
  }

  TestRunner.evaluateInPage('codeSnippetsNumber()', onTestNumberReceived);
})();
