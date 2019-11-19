// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests script snippet model.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('sources_test_runner');

  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML('<p></p>');

  const workspace = Workspace.workspace;
  SourcesTestRunner.runDebuggerTestSuite([
    async function testCreateEditRenameRemove(next) {
      function uiSourceCodeAdded(event) {
        var uiSourceCode = event.data;
        TestRunner.addResult('UISourceCodeAdded: ' + uiSourceCode.displayName());
      }

      function uiSourceCodeRemoved(event) {
        var uiSourceCode = event.data;
        TestRunner.addResult('UISourceCodeRemoved: ' + uiSourceCode.displayName());
      }

      async function printUiSourceCode(uiSourceCode) {
        const { content } = await uiSourceCode.requestContent();
        TestRunner.addResult(content);
      }

      workspace.addEventListener(
          Workspace.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);
      workspace.addEventListener(
          Workspace.Workspace.Events.UISourceCodeRemoved, uiSourceCodeRemoved);

      const uiSourceCode1 = await Snippets.project.createFile('', null, '');
      TestRunner.addResult('Snippet content:');
      await printUiSourceCode(uiSourceCode1);
      TestRunner.addResult('Snippet1 created.');
      const uiSourceCode2 = await Snippets.project.createFile('', null, '');
      TestRunner.addResult('Snippet content:');
      await printUiSourceCode(uiSourceCode2);
      TestRunner.addResult('Snippet2 created.');

      await rename(uiSourceCode1, 'foo');
      await rename(uiSourceCode1, '   ');
      await rename(uiSourceCode1, ' bar ');
      await rename(uiSourceCode1, 'foo');
      await rename(uiSourceCode2, 'bar');
      await rename(uiSourceCode2, 'foo');

      TestRunner.addResult('Content of first snippet:');
      await printUiSourceCode(uiSourceCode1);
      TestRunner.addResult('Content of second snippet:');
      await printUiSourceCode(uiSourceCode2);

      TestRunner.addResult('Delete snippets..');
      await uiSourceCode1.project().deleteFile(uiSourceCode1);
      await uiSourceCode2.project().deleteFile(uiSourceCode2);
      TestRunner.addResult(
          'Number of uiSourceCodes in workspace: ' +
          workspace.uiSourceCodes().filter(uiSourceCode => uiSourceCode.url().startsWith('snippet://')).length);

      TestRunner.addResult('Add third..');
      const uiSourceCode3 = await Snippets.project.createFile('', null, '');
      TestRunner.addResult('Content of third snippet:');
      await printUiSourceCode(uiSourceCode3);
      TestRunner.addResult(
          'Number of uiSourceCodes in workspace: ' +
          workspace.uiSourceCodes().filter(uiSourceCode => uiSourceCode.url().startsWith('snippet://')).length);
      TestRunner.addResult('Delete third..');
      await uiSourceCode3.project().deleteFile(uiSourceCode3);
      TestRunner.addResult(
          'Number of uiSourceCodes in workspace: ' +
          workspace.uiSourceCodes().filter(uiSourceCode => uiSourceCode.url().startsWith('snippet://')).length);
      workspace.removeEventListener(
          Workspace.Workspace.Events.UISourceCodeAdded, uiSourceCodeAdded);
      workspace.removeEventListener(
          Workspace.Workspace.Events.UISourceCodeRemoved,
          uiSourceCodeRemoved);
      next();

      async function rename(uiSourceCode, newName) {
        TestRunner.addResult(`Renaming snippet to '${newName}' ...`)
        const success = await uiSourceCode.rename(newName);
        TestRunner.addResult(success ? 'Snippet renamed successfully.' : 'Snippet was not renamed.');
        TestRunner.addResult(`UISourceCode name is '${uiSourceCode.name()}' now.`);
        TestRunner.addResult(
            'Number of uiSourceCodes in workspace: ' +
            workspace.uiSourceCodes().filter(uiSourceCode => uiSourceCode.url().startsWith('snippet://')).length);
      }
    },

    async function testEvaluate(next) {
      const uiSourceCode1 = await Snippets.project.createFile('', null, '');
      await uiSourceCode1.rename('Snippet1');
      uiSourceCode1.setWorkingCopy('// This snippet does nothing.\nvar i=2+2;\n');

      const uiSourceCode2 = await Snippets.project.createFile('', null, '');
      await uiSourceCode2.rename('Snippet2');
      uiSourceCode2.setWorkingCopy(`// This snippet creates a function that does nothing and returns it.
function doesNothing() {
  var i = 2+2;
};
doesNothing;
`);

      const uiSourceCode3 = await Snippets.project.createFile('', null, '');
      uiSourceCode3.rename('Snippet3');
      uiSourceCode3.setWorkingCopy('// This snippet uses Command Line API.\n$$("p").length');

      TestRunner.addResult('Run Snippet1..');
      Snippets.evaluateScriptSnippet(uiSourceCode1);
      await ConsoleTestRunner.waitUntilMessageReceivedPromise();
      ConsoleTestRunner.dumpConsoleMessages();

      const functionPromise = TestRunner.addSnifferPromise(
          Console.ConsoleViewMessage.prototype,
          '_formattedParameterAsFunctionForTest');
      TestRunner.addResult('Run Snippet2..');
      Snippets.evaluateScriptSnippet(uiSourceCode2);
      await ConsoleTestRunner.waitUntilMessageReceivedPromise();
      await functionPromise;
      ConsoleTestRunner.dumpConsoleMessages();

      TestRunner.addResult('Run Snippet1..');
      Snippets.evaluateScriptSnippet(uiSourceCode1);
      await ConsoleTestRunner.waitUntilMessageReceivedPromise();
      ConsoleTestRunner.dumpConsoleMessages();

      TestRunner.addResult('Run Snippet3..');
      Snippets.evaluateScriptSnippet(uiSourceCode3);
      await ConsoleTestRunner.waitUntilMessageReceivedPromise();
      ConsoleTestRunner.dumpConsoleMessages();

      await uiSourceCode1.project().deleteFile(uiSourceCode1);
      await uiSourceCode2.project().deleteFile(uiSourceCode2);
      await uiSourceCode3.project().deleteFile(uiSourceCode3);

      await TestRunner.evaluateInPagePromise('console.clear()');

      next();
    },

    async function testEvaluateEditReload(next) {
      const uiSourceCode1 = await Snippets.project.createFile('', null, '');
      await uiSourceCode1.rename('Snippet1');
      uiSourceCode1.setWorkingCopy('// This snippet does nothing.\nvar i=2+2;\n');

      TestRunner.addResult('Run Snippet1..');
      Snippets.evaluateScriptSnippet(uiSourceCode1);
      await ConsoleTestRunner.waitUntilMessageReceivedPromise();
      ConsoleTestRunner.dumpConsoleMessages();

      await TestRunner.reloadPagePromise();

      await uiSourceCode1.project().deleteFile(uiSourceCode1);
      next();
    },

    async function testEvaluateWithWorker(next) {
      TestRunner.addSniffer(
          SDK.RuntimeModel.prototype, '_executionContextCreated',
          contextCreated);
      TestRunner.evaluateInPagePromise(`
          var workerScript = "postMessage('Done.');";
          var blob = new Blob([workerScript], { type: 'text/javascript' });
          var worker = new Worker(URL.createObjectURL(blob));
      `);

      async function contextCreated() {
        // Take the only execution context from the worker's RuntimeModel.
        UI.context.setFlavor(SDK.ExecutionContext, this.executionContexts()[0]);

        const uiSourceCode1 = await Snippets.project.createFile('', null, '');
        await uiSourceCode1.rename('Snippet1');
        uiSourceCode1.setWorkingCopy('2 + 2');

        TestRunner.addResult('Run Snippet1..');
        Snippets.evaluateScriptSnippet(uiSourceCode1);
        await ConsoleTestRunner.waitUntilMessageReceivedPromise();
        ConsoleTestRunner.dumpConsoleMessages();

        await uiSourceCode1.project().deleteFile(uiSourceCode1);
        next();
      }
    },

    async function testDangerousNames(next) {
      const uiSourceCode1 = await Snippets.project.createFile('', null, '');
      await uiSourceCode1.rename('toString');
      await SourcesTestRunner.showUISourceCodePromise(uiSourceCode1);

      const uiSourceCode2 = await Snippets.project.createFile('', null, '');
      await uiSourceCode2.rename('myfile.toString');
      await SourcesTestRunner.showUISourceCodePromise(uiSourceCode2);

      await uiSourceCode1.project().deleteFile(uiSourceCode1);
      await uiSourceCode2.project().deleteFile(uiSourceCode2);
      next();
    }
  ]);
})();
