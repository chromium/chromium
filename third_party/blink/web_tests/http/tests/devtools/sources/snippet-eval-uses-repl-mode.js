// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Snippets from 'devtools/panels/snippets/snippets.js';

(async function() {
  TestRunner.addResult('Tests that snippet scripts are evaluated in REPL mode\n');

  await TestRunner.showPanel('sources');

  TestRunner.addSniffer(TestRunner.RuntimeAgent, 'invoke_evaluate', function(args) {
    TestRunner.addResult('Called RuntimeAgent.invoke_evaluate');
    TestRunner.addResult('Value of \'replMode\': ' + args.replMode);
  });

  const uiSourceCode = await Snippets.ScriptSnippetFileSystem.findSnippetsProject().createFile('', null, '');
  await uiSourceCode.rename('Snippet1');
  uiSourceCode.setWorkingCopy('let a = 1; let a = 2;');

  await Snippets.ScriptSnippetFileSystem.evaluateScriptSnippet(uiSourceCode);

  TestRunner.completeTest();
})();
