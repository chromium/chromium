// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.setupStartupTest('resources/linkifier.html');
  TestRunner.addResult(`Tests that Linkifier works correctly.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  var resourceURL = TestRunner.url('resources/linkifier.html');

  var scriptFormatter;
  var linkifier;
  var link;
  var script;
  var uiSourceCode;

  SourcesTestRunner.scriptFormatter().then(startDebuggerTest);

  function startDebuggerTest(sf) {
    scriptFormatter = sf;
    SourcesTestRunner.startDebuggerTest(waitForScripts);
  }

  function waitForScripts() {
    SourcesTestRunner.showScriptSource('linkifier.html', debuggerTest);
  }

  function debuggerTest() {
    for (var scriptCandidate of TestRunner.debuggerModel.scripts()) {
      if (scriptCandidate.sourceURL === resourceURL && scriptCandidate.lineOffset === 4) {
        script = scriptCandidate;
        break;
      }
    }

    uiSourceCode = Workspace.workspace.uiSourceCodeForURL(resourceURL);

    linkifier = new Components.Linkifier();
    var count1 = liveLocationsCount();
    link = linkifier.linkifyScriptLocation(
        SDK.targetManager.mainTarget(), null, resourceURL, 8, 0, 'dummy-class');
    var count2 = liveLocationsCount();

    TestRunner.addResult('listeners added on raw source code: ' + (count2 - count1));
    TestRunner.addResult('original location: ' + link.textContent);
    TestRunner.addSniffer(Sources.ScriptFormatterEditorAction.prototype, '_updateButton', uiSourceCodeScriptFormatted);
    scriptFormatter._toggleFormatScriptSource();
  }

  async function uiSourceCodeScriptFormatted() {
    TestRunner.addResult('pretty printed location: ' + link.textContent);
    var formattedContent = (await Sources.sourceFormatter._formattedSourceCodes.get(uiSourceCode).formatData.formattedSourceCode.requestContent()).content;
    TestRunner.addResult('pretty printed content:');
    TestRunner.addResult(formattedContent);
    Sources.sourceFormatter.discardFormattedUISourceCode(UI.panels.sources.visibleView.uiSourceCode());
    TestRunner.addResult('reverted location: ' + link.textContent);

    var count1 = liveLocationsCount();
    linkifier.reset();
    var count2 = liveLocationsCount();

    TestRunner.addResult('listeners removed from raw source code: ' + (count1 - count2));

    SourcesTestRunner.completeDebuggerTest();
  }

  function liveLocationsCount() {
    var modelData = Bindings.debuggerWorkspaceBinding._debuggerModelToData.get(script.debuggerModel);
    var locations = modelData._locations.get(script);
    return locations.size;
  }
})();
