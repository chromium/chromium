// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Bindings from 'devtools/models/bindings/bindings.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests ResourceScriptMapping class.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function loadIframe()
      {
          var iframe = document.createElement("iframe");
          iframe.setAttribute("src", "resources/multiple-scripts.html");
          document.body.appendChild(iframe);
      }
  `);

  var url = TestRunner.url('resources/multiple-scripts.html');
  var scripts = [];
  var count = 3;

  TestRunner.addResult('Waiting for scripts');
  TestRunner.debuggerModel.addEventListener(SDK.DebuggerModel.Events.ParsedScriptSource, onScriptParsed);
  TestRunner.evaluateInPage('loadIframe()');

  function onScriptParsed(event) {
    var script = event.data;
    if (script.sourceURL !== url)
      return;
    TestRunner.addResult('Script arrived');
    scripts.push(script);
    if (!--count) {
      TestRunner.debuggerModel.removeEventListener(SDK.DebuggerModel.Events.ParsedScriptSource, onScriptParsed);
      TestRunner.addResult('Waiting for UISourceCode');
      TestRunner.waitForUISourceCode(url).then(onUISourceCode);
    }
  }

  async function onUISourceCode(uiSourceCode) {
    TestRunner.addResult('UISourceCode arrived');
    scripts.sort((s1, s2) => {
      return s1.lineOffset - s2.lineOffset;
    });
    for (var script of scripts) {
      TestRunner.addResult(`Checking script at (${script.lineOffset}, ${script.columnOffset})`);
      var line = script.lineOffset;
      var column = script.columnOffset + 2;
      var rawLocation = TestRunner.debuggerModel.createRawLocation(script, line, column);
      var uiLocation = await Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().rawLocationToUILocation(rawLocation);
      SourcesTestRunner.checkUILocation(uiSourceCode, line, column, uiLocation);
      var reverseLocation = (await Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().uiLocationToRawLocations(uiSourceCode, line, column))[0];
      SourcesTestRunner.checkRawLocation(script, line, column, reverseLocation);
    }
    TestRunner.completeTest();
  }
})();
