// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests stepping out from custom element callbacks.\n`);

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
          debugger;
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
      var foo = new FooElement();
      foo.setAttribute('x', 'b');
      document.body.appendChild(foo);
      foo.remove();
    }
  `);

  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    // clang-format off
    var actions = [
      'Print',              // debugger; in FooElement constructor
      'StepOut', 'Print',   // at foo.setAttribute()
      'StepInto', 'Print',  // at attributeChangedCallback
      'StepOut', 'Print',   // at document.body.appendChild()
      'StepInto', 'Print',  // at connectedCallback
      'StepOut', 'Print',   // at foo.remove()
      'StepInto', 'Print',  // at disconnectedCallback
      'StepOut', 'Print',   // at testFunction() return point
    ];
    // clang-format on

    SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(actions, step3);
  }

  function step3() {
    SourcesTestRunner.resumeExecution(step4);
  }

  async function step4() {
    const output = await TestRunner.evaluateInPageAsync('JSON.stringify(self._output)');
    TestRunner.addResults(JSON.parse(output));
    TestRunner.completeTest();
  }
})();
