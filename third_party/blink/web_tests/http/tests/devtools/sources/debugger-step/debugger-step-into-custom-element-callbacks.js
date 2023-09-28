// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests that stepping into custom element methods will lead to a pause in the callbacks.\n`);

  await TestRunner.showPanel('sources');

  await TestRunner.evaluateInPagePromise(`
    function output(message) {
      if (!self._output)
          self._output = [];
      self._output.push('[page] ' + message);
    }

    function testFunction()
    {
      class FooElement extends HTMLElement {
        constructor() {
          super();
          output('Invoked constructor.');
        }
        connectedCallback() {
          output('Invoked connectedCallback.');
        }
        disconnectedCallback() {
          output('Invoked disconnectedCallback.');
        }
        adoptedCallback() {
          output('Invoked adoptedCallback.');
        }
        attributeChangedCallback() {
          output('Invoked attributeChangedCallback.');
        }
        static get observedAttributes() { return ['x']; }
      }
      customElements.define('x-foo', FooElement);
      debugger;
      var foo = new FooElement();
      debugger;
      foo.setAttribute('x', 'b');
      debugger;
      document.body.appendChild(foo);
      debugger;
      foo.remove();
    }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function checkTopFrameFunction(callFrames, expectedName) {
    var topFunctionName = callFrames[0].functionName;

    if (expectedName === topFunctionName)
      TestRunner.addResult('PASS: Did step into event listener(' + expectedName + ').');
    else
      TestRunner.addResult('FAIL: Unexpected top function: expected ' + expectedName + ', found ' + topFunctionName);
  }

  function stepOverThenIn(name, callback) {
    TestRunner.addResult('Stepping to ' + name + '...');
    SourcesTestRunner.stepOver();

    SourcesTestRunner.waitUntilResumed(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, function() {
      TestRunner.addResult('Stepping into ' + name + '...');
      SourcesTestRunner.stepInto();
      SourcesTestRunner.waitUntilResumed(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, callback));
    }));
  }

  function step2() {
    stepOverThenIn('constructor', step3);
  }

  function step3(callFrames) {
    checkTopFrameFunction(callFrames, 'FooElement');
    SourcesTestRunner.resumeExecution(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, step4));
  }

  function step4() {
    stepOverThenIn('setAttribute', step5);
  }

  function step5(callFrames) {
    checkTopFrameFunction(callFrames, 'attributeChangedCallback');
    SourcesTestRunner.resumeExecution(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, step6));
  }

  function step6() {
    stepOverThenIn('connectedCallback', step7);
  }

  function step7(callFrames) {
    checkTopFrameFunction(callFrames, 'connectedCallback');
    SourcesTestRunner.resumeExecution(SourcesTestRunner.waitUntilPaused.bind(SourcesTestRunner, step8));
  }

  function step8() {
    stepOverThenIn('disconnectedCallback', step9);
  }

  function step9(callFrames) {
    checkTopFrameFunction(callFrames, 'disconnectedCallback');
    SourcesTestRunner.resumeExecution(step10);
  }

  async function step10() {
    const output = await TestRunner.evaluateInPageAsync('JSON.stringify(self._output)');
    TestRunner.addResults(JSON.parse(output));
    TestRunner.completeTest();
  }
})();
