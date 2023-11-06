// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';
import * as Persistence from 'devtools/models/persistence/persistence.js';
import * as Console from 'devtools/panels/console/console.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(
      'Verifies that modules can be loaded via import() in snippets\n');
  await TestRunner.showPanel('sources');

  const sourceCode = `
      (async() => {
        const myModule = await import('./resources/module.js');
        console.log('myModule.message: ' + myModule.message);
      })()
      'end of snippet'`;

  const projects =
      Workspace.Workspace.WorkspaceImpl.instance().projectsForType(Workspace.Workspace.projectTypes.FileSystem);
  const snippetsProject = projects.find(
      project => Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.fileSystemType(
                     project) === 'snippets');
  const uiSourceCode = await snippetsProject.createFile('');

  uiSourceCode.setContent(sourceCode);
  await Common.Revealer.reveal(uiSourceCode);
  await uiSourceCode.rename('my_snippet_name');
  SourcesModule.SourcesPanel.SourcesPanel.instance().runSnippet();

  await ConsoleTestRunner.waitUntilNthMessageReceivedPromise(2);
  await ConsoleTestRunner.dumpConsoleMessages();
  Console.ConsoleView.ConsoleView.clearConsole();
  TestRunner.completeTest();
})();
