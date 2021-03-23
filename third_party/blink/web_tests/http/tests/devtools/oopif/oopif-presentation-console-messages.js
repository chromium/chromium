// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that links to UISourceCode work correctly when navigating OOPIF`);

  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  function dumpMessage(prefix, message) {
    TestRunner.addResult(`Line Message was ${prefix}: ${message.uiSourceCode().url()} ${message.level()} '${message.text()}':${message.lineNumber()}:${message.columnNumber()}`);
  }
  TestRunner.addSniffer(Workspace.UISourceCode.prototype, 'addLineMessage', (level, text, lineNumber, columnNumber, message) => dumpMessage('added', message), true);
  TestRunner.addSniffer(Workspace.UISourceCode.prototype, 'removeMessage', message => dumpMessage('removed', message), true);

  TestRunner.addResult('\nNavigating main frame');
  await TestRunner.navigatePromise('resources/error.html');
  TestRunner.addResult('Revealing main frame source');
  await Common.Revealer.reveal(Workspace.workspace.uiSourceCodeForURL('http://127.0.0.1:8000/devtools/oopif/resources/error.html'));
  TestRunner.addResult('\nCreating iframe');
  await TestRunner.addIframe('http://devtools.oopif.test:8000/devtools/oopif/resources/error.html', {id: 'myframe'});
  TestRunner.addResult('Revealing iframe source');
  await Common.Revealer.reveal(Workspace.workspace.uiSourceCodeForURL('http://devtools.oopif.test:8000/devtools/oopif/resources/error.html'));
  TestRunner.addResult('\nNavigating iframe');
  await TestRunner.evaluateInPageAsync(`
    (function() {
      var iframe = document.getElementById('myframe');
      iframe.src = 'http://devtools.oopif.test:8000/devtools/oopif/resources/empty.html';
      return new Promise(f => iframe.onload = f);
    })()
  `);
  TestRunner.addResult('Revealing iframe source');
  await Common.Revealer.reveal(Workspace.workspace.uiSourceCodeForURL('http://devtools.oopif.test:8000/devtools/oopif/resources/empty.html'));
  TestRunner.addResult('\nClearing console');
  SDK.consoleModel.requestClearMessages();
  TestRunner.completeTest();
})();
