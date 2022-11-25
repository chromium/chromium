// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that snippet scripts are evaluated in REPL mode\n');

  await TestRunner.loadLegacyModule('snippets');
  await TestRunner.showPanel('sources');

  TestRunner.addSniffer(TestRunner.RuntimeAgent, 'invoke_evaluate', function(args) {
    TestRunner.addResult('Called RuntimeAgent.invoke_evaluate');
    TestRunner.addResult('Value of \'replMode\': ' + args.replMode);
  });

  const uiSourceCode = await Snippets.ScriptSnippetFileSystem.findSnippetsProject().createFile('', null, '');
  await uiSourceCode.rename('Snippet1');
  uiSourceCode.setWorkingCopy('let a = 1; let a = 2;');

  await Snippets.evaluateScriptSnippet(uiSourceCode);

  TestRunner.completeTest();
})();
