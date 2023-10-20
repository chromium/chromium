// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Bindings from 'devtools/models/bindings/bindings.js'
import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Tests SourceMap and CompilerScriptMapping.\n`);
  await TestRunner.evaluateInPagePromise(`
      function addScript(url)
      {
          var script = document.createElement("script");
          script.setAttribute("src", url);
          document.head.appendChild(script);
      }
  `);

  function uiLocation(script, line, column) {
    var location = script.debuggerModel.createRawLocation(script, line, column);
    return Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().rawLocationToUILocation(location);
  }

  TestRunner.runTestSuite([
    function testCompilerScriptMapping(next) {
      var script;
      var originalUISourceCode;
      var target = TestRunner.mainTarget;
      var debuggerModel = TestRunner.debuggerModel;

      TestRunner.addResult('Adding compiled.js');
      TestRunner.waitForUISourceCode('resources/compiled.js').then(originalUISourceCodeAdded);
      TestRunner.evaluateInPage('addScript(\'resources/compiled.js\')');

      function originalUISourceCodeAdded(uiSourceCode) {
        TestRunner.addResult('compiled.js UISourceCode arrived');
        originalUISourceCode = uiSourceCode;
        for (var s of debuggerModel.scripts()) {
          if (s.sourceURL.endsWith('compiled.js')) {
            TestRunner.addResult('compiled.js script found');
            script = s;
          }
        }
        TestRunner.waitForUISourceCode('source1.js').then(firstUISourceCodeAdded);
      }

      function firstUISourceCodeAdded(uiSourceCode) {
        TestRunner.addResult('source1.js UISourceCode arrived');
        TestRunner.waitForUISourceCode('source2.js').then(secondUISourceCodeAdded);
      }

      async function secondUISourceCodeAdded(uiSourceCode) {
        TestRunner.addResult('source2.js UISourceCode arrived');
        var uiSourceCode1 =
            Workspace.Workspace.WorkspaceImpl.instance().uiSourceCodeForURL('http://127.0.0.1:8000/devtools/resources/source1.js');
        var uiSourceCode2 =
            Workspace.Workspace.WorkspaceImpl.instance().uiSourceCodeForURL('http://127.0.0.1:8000/devtools/resources/source2.js');

        SourcesTestRunner.checkUILocation(uiSourceCode1, 4, 4, await uiLocation(script, 0, 81));
        SourcesTestRunner.checkUILocation(uiSourceCode1, 5, 4, await uiLocation(script, 0, 93));
        SourcesTestRunner.checkUILocation(uiSourceCode2, 7, 4, await uiLocation(script, 1, 151));
        SourcesTestRunner.checkUILocation(originalUISourceCode, 1, 200, await uiLocation(script, 1, 200));

        SourcesTestRunner.checkRawLocation(
            script, 0, 48, (await Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().uiLocationToRawLocations(uiSourceCode1, 3, 10))[0]);
        SourcesTestRunner.checkRawLocation(
            script, 1, 85, (await Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().uiLocationToRawLocations(uiSourceCode2, 1, 0))[0]);
        SourcesTestRunner.checkRawLocation(
            script, 1, 140, (await Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().uiLocationToRawLocations(uiSourceCode2, 5, 2))[0]);

        TestRunner.addResult('Location checks passed. Requesting content');
        uiSourceCode1.requestContent().then(didRequestContent1);

        function didRequestContent1({ content, error, isEncoded }) {
          TestRunner.addResult('Content1 arrived.');
          TestRunner.assertEquals(0, content.indexOf('window.addEventListener'));
          uiSourceCode2.requestContent().then(didRequestContent2);
        }

        function didRequestContent2({ content, error, isEncoded }) {
          TestRunner.addResult('Content2 arrived.');
          TestRunner.assertEquals(0, content.indexOf('function ClickHandler()'));
          next();
        }
      }
    },

    function testInlinedSourceMap(next) {
      var sourceMap = {
        'file': 'compiled.js',
        'mappings': 'AAASA,QAAAA,IAAG,CAACC,CAAD,CAAaC,CAAb,CACZ,CACI,MAAOD,EAAP,CAAoBC,CADxB,CAIA,IAAIC,OAAS;',
        'sources': ['source3.js'],
        'sourcesContent': ['<source content>']
      };
      var sourceMapURL = 'data:application/json;base64,' + btoa(JSON.stringify(sourceMap));
      var scriptSource = '\n//# sourceMappingURL=' + sourceMapURL + '\n';

      TestRunner.addResult('Adding compiled.js');
      TestRunner.waitForUISourceCode().then(compiledUISourceCodeAdded);
      TestRunner.evaluateInPage(scriptSource);

      var target = TestRunner.mainTarget;
      var debuggerModel = TestRunner.debuggerModel;
      var script;

      function compiledUISourceCodeAdded(uiSourceCode) {
        TestRunner.addResult('compiled.js UISourceCode arrived');
        for (var s of debuggerModel.scripts()) {
          if (s.sourceMapURL && s.sourceMapURL.startsWith('data:application')) {
            TestRunner.addResult('compiled.js script found');
            script = s;
          }
        }
        TestRunner.waitForUISourceCode('source3.js').then(originalUISourceCodeAdded);
      }

      async function originalUISourceCodeAdded(uiSourceCode) {
        TestRunner.addResult('source3.js UISourceCode arrived');
        SourcesTestRunner.checkUILocation(uiSourceCode, 2, 4, await uiLocation(script, 0, 18));
        SourcesTestRunner.checkRawLocation(
            script, 0, 18, (await Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance().uiLocationToRawLocations(uiSourceCode, 2, 4))[0]);

        TestRunner.addResult('Location checks passed. Requesting content');
        uiSourceCode.requestContent().then(didRequestContent);

        function didRequestContent({ content, error, isEncoded }) {
          TestRunner.addResult('<source content> === ' + content);
          next();
        }
      }
    },

    function testSourceMapCouldNotBeLoaded(next) {
      TestRunner.addResult('Adding compiled.js');
      TestRunner.evaluateInPage('\n//# sourceMappingURL=http://127.0.0.1:8000/devtools/resources/source-map.json_\n');
      TestRunner.debuggerModel.sourceMapManager().addEventListener(
          SDK.SourceMapManager.Events.SourceMapFailedToAttach, onSourceMapLoaded, this);

      async function onSourceMapLoaded(event) {
        var script = event.data.client;
        if (script.sourceMapURL !== 'http://127.0.0.1:8000/devtools/resources/source-map.json_')
          return;
        TestRunner.addResult('SourceMap Failed to load.');
        var location = await uiLocation(script, 0, 0);
        TestRunner.addResult(location.uiSourceCode.url().replace(/VM\d+/g, 'VM'));
        next();
      }
    }
  ]);
})();
