// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

(async function() {
  TestRunner.addResult(`Tests long script content is correctly shown in source panel after page reload.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.showPanel('network');
  await TestRunner.navigatePromise('resources/long-script-page.html');
  TestRunner.hardReloadPage(step1);

  function step1() {
    ConsoleTestRunner.addConsoleSniffer(step2);
    TestRunner.evaluateInPage('loadScript()');
  }

  function step2(event) {
    TestRunner.evaluateInPage('unloadScript()', step3);
  }

  function step3() {
    SourcesTestRunner.waitForScriptSource('long_script.cgi', step4);
  }

  function step4(uiSourceCode) {
    TestRunner.evaluateInPage('gc()', step5.bind(null, uiSourceCode));
  }

  function step5(uiSourceCode) {
    uiSourceCode.requestContent().then(step6);
  }

  function step6({ content, error, isEncoded }) {
    let loadedScript = content;
    var expected = 'console.log(\'finished\');\n';
    TestRunner.assertTrue(!!loadedScript, 'No script content');
    loadedScript = loadedScript.replace(/\r\n/g, '\n');  // on windows we receive additional symbol \r at line end.
    TestRunner.assertEquals(1024 * 10240 + expected.length, loadedScript.length, 'Loaded script length mismatch');
    var actual = loadedScript.substring(loadedScript.length - expected.length);
    TestRunner.assertEquals(expected, actual, 'Loaded script is corrupted');

    TestRunner.addResult('Test passed');

    TestRunner.completeTest();
  }
})();
