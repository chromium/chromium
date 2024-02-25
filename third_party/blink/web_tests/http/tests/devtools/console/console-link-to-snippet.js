// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SourcesModule from 'devtools/panels/sources/sources.js';
import * as Persistence from 'devtools/models/persistence/persistence.js';
import * as Console from 'devtools/panels/console/console.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Test that link to snippet works.\n`);

  await TestRunner.showPanel('console');

  TestRunner.addSniffer(
      Workspace.UISourceCode.UISourceCode.prototype, 'addMessage', dumpLineMessage, true);

  TestRunner.runTestSuite([
    function testConsoleLogAndReturnMessageLocation(next) {
      ConsoleTestRunner.waitUntilNthMessageReceivedPromise(2)
        .then(() => ConsoleTestRunner.dumpConsoleMessages())
        .then(() => Console.ConsoleView.ConsoleView.clearConsole())
        .then(() => next());

      createSnippetPromise('console.log(239);42')
        .then(uiSourceCode => selectSourceCode(uiSourceCode))
        .then(uiSourceCode => renameSourceCodePromise('name1', uiSourceCode))
        .then(() => runSelectedSnippet());
    },

    function testSnippetSyntaxError(next) {
      ConsoleTestRunner.waitUntilNthMessageReceivedPromise(1)
        .then(() => ConsoleTestRunner.dumpConsoleMessages())
        .then(() => Console.ConsoleView.ConsoleView.clearConsole())
        .then(() => next());

      createSnippetPromise('\n }')
        .then(uiSourceCode => selectSourceCode(uiSourceCode))
        .then(uiSourceCode => renameSourceCodePromise('name2', uiSourceCode))
        .then(() => runSelectedSnippet());
    },

    function testConsoleErrorHighlight(next) {
      ConsoleTestRunner.waitUntilNthMessageReceivedPromise(4)
        .then(() => ConsoleTestRunner.dumpConsoleMessages())
        .then(() => Console.ConsoleView.ConsoleView.clearConsole())
        .then(() => next());

      createSnippetPromise(`
console.error(42);
console.error(-0);
console.error(false);
console.error(null)`)
        .then(uiSourceCode => selectSourceCode(uiSourceCode))
        .then(uiSourceCode => renameSourceCodePromise('name3', uiSourceCode))
        .then(() => runSelectedSnippet());
    }
  ]);

  async function createSnippetPromise(content) {
    const projects = Workspace.Workspace.WorkspaceImpl.instance().projectsForType(Workspace.Workspace.projectTypes.FileSystem);
    const snippetsProject = projects.find(project => Persistence.FileSystemWorkspaceBinding.FileSystemWorkspaceBinding.fileSystemType(project) === 'snippets');
    const uiSourceCode = await snippetsProject.createFile('');
    uiSourceCode.setContent(content);
    return uiSourceCode;
  }

  function renameSourceCodePromise(newName, uiSourceCode) {
    var callback;
    var promise = new Promise(fullfill => (callback = fullfill));
    uiSourceCode.rename(newName).then(() => callback(uiSourceCode));
    return promise;
  }

  function selectSourceCode(uiSourceCode) {
    return Common.Revealer.reveal(uiSourceCode).then(() => uiSourceCode);
  }

  function dumpLineMessage(message) {
    TestRunner.addResult(`Line Message was added: ${this.url()} ${
        message.level()} '${message.text()}':${message.lineNumber()}:${
        message.columnNumber()}`);
  }

  function runSelectedSnippet() {
    SourcesModule.SourcesPanel.SourcesPanel.instance().runSnippet();
  }
})();
