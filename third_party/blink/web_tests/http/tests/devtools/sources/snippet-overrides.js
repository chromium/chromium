// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

(async function() {
  TestRunner.addResult(
      'Verifies that snippets still map when local overrides is on.\n');
  await TestRunner.loadLegacyModule('sources');
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
  Sources.SourcesPanel.instance().runSnippet();
  const binding = await bindingPromise;
  TestRunner.addResult(binding.network.url() + ' <=> ' + binding.fileSystem.url());

  TestRunner.completeTest();
})();
