// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that scripts list is cleared upon page reload.\n`);
  await TestRunner.showPanel('sources');

  TestRunner.evaluateInPage('function foo() {} //# sourceURL=dummyScript.js', step1);

  function step1() {
    SourcesTestRunner.startDebuggerTest(step2);
  }

  function step2() {
    SourcesTestRunner.queryScripts(function(script) {
      step3({data: script});
    });
    TestRunner.debuggerModel.addEventListener(SDK.DebuggerModel.Events.ParsedScriptSource, step3);
  }

  function step3(event) {
    var script = event.data;
    if (script.sourceURL.indexOf('dummyScript.js') !== -1) {
      TestRunner.addResult('Dummy script found: ' + script.sourceURL);
      // Let scripts dispatch and reload.
      setTimeout(TestRunner.reloadPage.bind(TestRunner, afterReload), 0);
    }
  }

  function afterReload() {
    var scripts = SourcesTestRunner.queryScripts();
    for (var i = 0; i < scripts.length; ++i) {
      if (scripts[i].sourceURL.indexOf('dummyScript.js') !== -1)
        TestRunner.addResult('FAILED: dummy script found after navigation');
    }
    SourcesTestRunner.completeDebuggerTest();
  }
})();
