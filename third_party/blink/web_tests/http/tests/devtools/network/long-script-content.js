// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests long script content is correctly shown in source panel after page reload.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('console_test_runner');
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
