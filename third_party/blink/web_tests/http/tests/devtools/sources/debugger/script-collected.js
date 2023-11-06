// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(
      `Tests that DiscardedAnonymousScriptSource event is fired and workspace is cleared.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function generateErrorScripts()
      {
          for (var i = 0; i < 2000; ++i) {
              try {
                eval("#BAD SCRIPT# " + i);
              } catch(e) {
              }
          }
      }
  `);

  SourcesTestRunner.startDebuggerTest(step1);
  var discardedScripts = 0;

  function step1() {
    TestRunner.debuggerModel.addEventListener(
        SDK.DebuggerModel.Events.DiscardedAnonymousScriptSource,
        () => ++discardedScripts);
    TestRunner.evaluateInPage(
        'generateErrorScripts()\n//# sourceURL=foo', step2);
  }

  function step2() {
    TestRunner.addResult('Discarded: ' + discardedScripts);
    var codes =
        Workspace.Workspace.WorkspaceImpl.instance()
            .uiSourceCodesForProjectType(Workspace.Workspace.projectTypes.Debugger)
            .filter(code => !code.url().match(/VM\d+\s/));
    TestRunner.addResult('Remaining UISourceCodes: ' + codes.length);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
