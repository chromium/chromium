// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that valid parsed script notifications are received by front-end.`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise('resources/debugger-scripts.html');

  var scripts = [];
  SourcesTestRunner.startDebuggerTest(step1);

  function step1() {
    SourcesTestRunner.queryScripts(function(script) {
      step2({data: script});
    });
    TestRunner.debuggerModel.addEventListener(
        SDK.DebuggerModel.Events.ParsedScriptSource, step2);
  }

  function step2(event) {
    var script = event.data;
    if (script.sourceURL !== TestRunner.mainTarget.inspectedURL())
      return;
    scripts.push(script);
    if (scripts.length === 4)
      step3();
  }

  function step3() {
    scripts.sort(function(x, y) {
      return x.lineOffset - y.lineOffset;
    });
    for (var i = 0; i < scripts.length; ++i) {
      TestRunner.addResult('script ' + (i + 1) + ':');
      TestRunner.addResult(
          '    start: ' + scripts[i].lineOffset + ':' +
          scripts[i].columnOffset);
      TestRunner.addResult(
          '    end: ' + scripts[i].endLine + ':' + scripts[i].endColumn);
    }
    SourcesTestRunner.completeDebuggerTest();
  }
})();
