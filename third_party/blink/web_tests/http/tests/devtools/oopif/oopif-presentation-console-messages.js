// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that links to UISourceCode work correctly when navigating OOPIF`);

  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  const messages = new Map();
  function dumpMessages() {
    TestRunner.addResult("Current messages:");
    for(const [message, url] of messages.entries()) {
      TestRunner.addResult(`  - ${url} ${
        message.level()} '${message.text()}':${message.lineNumber()}:${
        message.columnNumber()}`);
    }
  }
  TestRunner.addSniffer(
      Workspace.UISourceCode.prototype, 'addMessage', function(message) {
        messages.set(message, this.url());
      }, true);
  TestRunner.addSniffer(
      Workspace.UISourceCode.prototype, 'removeMessage', function(message) {
        messages.delete(message, this.url());
      }, true);

  TestRunner.addResult('\nNavigating main frame');
  await TestRunner.navigatePromise('resources/error.html');
  dumpMessages();
  TestRunner.addResult('Revealing main frame source');
  await Common.Revealer.reveal(Workspace.workspace.uiSourceCodeForURL('http://127.0.0.1:8000/devtools/oopif/resources/error.html'));
  TestRunner.addResult('\nCreating iframe');
  await TestRunner.addIframe('http://devtools.oopif.test:8000/devtools/oopif/resources/error.html', {id: 'myframe'});
  dumpMessages();
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
  dumpMessages();
  TestRunner.addResult('Revealing iframe source');
  await Common.Revealer.reveal(Workspace.workspace.uiSourceCodeForURL('http://devtools.oopif.test:8000/devtools/oopif/resources/empty.html'));
  TestRunner.addResult('\nClearing console');
  SDK.consoleModel.requestClearMessages();
  dumpMessages();
  TestRunner.completeTest();
})();
