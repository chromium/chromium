// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests framework blackboxing feature on jQuery.\n`);

  await TestRunner.showPanel('sources');

  await TestRunner.loadHTML(`
    <input type="button" onclick="testFunction()" value="Test">
    <p></p>
  `);

  await TestRunner.addScriptTag('../debugger/resources/jquery-1.11.1.min.js');

  await TestRunner.evaluateInPagePromise(`
    function output(message) {
      if (!self._output)
          self._output = [];
      self._output.push('[page] ' + message);
    }

    function testFunction()
    {
        var pp = $("p");
        var scripts = $("script");
        pp.on("testevent", onTestEvent1);
        pp.on("testevent", onTestEvent2);

        debugger;

        scripts.each(onEachScript);
        pp.trigger("testevent");
    }

    function onTestEvent1()
    {
        output("onTestEvent1");
    }

    function onTestEvent2()
    {
        output("onTestEvent2");
    }

    function onEachScript(index, script)
    {
        return script.textContent;
    }
  `);

  var frameworkRegexString = '/jquery-1\\.11\\.1\\.min\\.js$';
  Common.Settings.settingForTest('skip-stack-frames-pattern').set(frameworkRegexString);
  SourcesTestRunner.startDebuggerTest(step1, true);

  function step1() {
    SourcesTestRunner.runTestFunctionAndWaitUntilPaused(step2);
  }

  function step2() {
    // clang-format off
    var actions = [
      'Print',                          // "debugger" in testFunction()
      'StepInto', 'StepInto', 'Print',  // entered onEachScript()
      'StepOut',  'Print',              // about to execute jQuery.trigger()
      'StepInto', 'Print',              // onTestEvent1
      'StepOver', 'StepOver', 'Print'   // onTestEvent2
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
