// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests that custom element lifecycle events fire while debugger is paused.\n`);

  await TestRunner.showPanel('sources');

  await TestRunner.evaluateInPagePromise(`
    function output(message) {
      if (!self._output)
          self._output = [];
      self._output.push('[page] ' + message);
    }
  `);

  var setup = `
      class CustomElement extends HTMLElement {
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
      customElements.define('x-foo', CustomElement);
    `;

  var lifecycleCallbacks = `
    created = new CustomElement();
    created.setAttribute('x', '1');
    document.body.appendChild(created);
    created.remove();
  `;

  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    ConsoleTestRunner.evaluateInConsole(setup, function() {
      TestRunner.addResult('Custom element registered.');
      ConsoleTestRunner.evaluateInConsoleAndDump('new CustomElement() instanceof CustomElement', step2);
    });
  }

  function step2() {
    ConsoleTestRunner.evaluateInConsole('debugger;');
    SourcesTestRunner.waitUntilPaused(step3);
  }

  function step3() {
    ConsoleTestRunner.evaluateInConsoleAndDump('new CustomElement() instanceof CustomElement', step4);
  }

  function step4() {
    ConsoleTestRunner.evaluateInConsole(lifecycleCallbacks, step5);
  }

  function step5() {
    SourcesTestRunner.resumeExecution(step6);
  }

  async function step6() {
    const output = await TestRunner.evaluateInPageAsync('JSON.stringify(self._output)');
    TestRunner.addResults(JSON.parse(output));
    TestRunner.completeTest();
  }
})();
