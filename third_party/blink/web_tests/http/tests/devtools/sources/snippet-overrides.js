// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      'Verifies that snippets still map when local overrides is on.\n');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('bindings_test_runner');
  await TestRunner.showPanel('sources');

  await BindingsTestRunner.createOverrideProject('file:///tmp');
  BindingsTestRunner.setOverridesEnabled(true);

  const sourceCode = `'hello';`;

  const projects =
      Workspace.workspace.projectsForType(Workspace.projectTypes.FileSystem);
  const snippetsProject = projects.find(
      project => Persistence.FileSystemWorkspaceBinding.fileSystemType(
                     project) === 'snippets');
  const uiSourceCode = await snippetsProject.createFile('');

  uiSourceCode.setContent(sourceCode);
  await Common.Revealer.reveal(uiSourceCode);
  await uiSourceCode.rename('my_snippet_name');
  const bindingPromise = Persistence.persistence.once(Persistence.Persistence.Events.BindingCreated);
  Sources.SourcesPanel.instance()._runSnippet();
  const binding = await bindingPromise;
  TestRunner.addResult(binding.network.url() + ' <=> ' + binding.fileSystem.url());

  TestRunner.completeTest();
})();
