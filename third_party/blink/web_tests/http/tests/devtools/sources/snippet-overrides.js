// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';
import * as Persistence from 'devtools/models/persistence/persistence.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(
      'Verifies that snippets still map when local overrides is on.\n');
  await TestRunner.showPanel('sources');

  await BindingsTestRunner.createOverrideProject('file:///tmp');
  BindingsTestRunner.setOverridesEnabled(true);

  const sourceCode = `'hello';`;

  const projects =
      Workspace.Workspace.WorkspaceImpl.instance().projectsForType(Workspace.Workspace.projectTypes.FileSystem);
  const snippetsProject = projects.find(
      project => Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.fileSystemType(
                     project) === 'snippets');
  const uiSourceCode = await snippetsProject.createFile('');

  uiSourceCode.setContent(sourceCode);
  await Common.Revealer.reveal(uiSourceCode);
  await uiSourceCode.rename('my_snippet_name');
  const bindingPromise = Persistence.Persistence.PersistenceImpl.instance().once(Persistence.Persistence.Events.BindingCreated);
  SourcesModule.SourcesPanel.SourcesPanel.instance().runSnippet();
  const binding = await bindingPromise;
  TestRunner.addResult(binding.network.url() + ' <=> ' + binding.fileSystem.url());

  TestRunner.completeTest();
})();
