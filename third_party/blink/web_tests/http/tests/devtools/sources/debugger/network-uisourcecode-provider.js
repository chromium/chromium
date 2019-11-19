// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests NetworkUISourceCodeProvider class.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function removeStyleSheet()
      {
          let css = document.querySelector('link');
          css.parentNode.removeChild(css);
          window.getComputedStyle(document.body).color;
      }
  `);

  var target = TestRunner.mainTarget;

  function uiSourceCodeURL(uiSourceCode) {
    return uiSourceCode.url().replace(/.*(LayoutTests|web_tests)./, '');
  }

  function dumpUISourceCode(uiSourceCode, callback) {
    TestRunner.addResult('UISourceCode: ' + uiSourceCodeURL(uiSourceCode));
    if (uiSourceCode.contentType() === Common.resourceTypes.Script ||
        uiSourceCode.contentType() === Common.resourceTypes.Document)
      TestRunner.addResult(
          'UISourceCode is content script: ' +
          (uiSourceCode.project().type() ===
           Workspace.projectTypes.ContentScripts));
    uiSourceCode.requestContent().then(didRequestContent);

    function didRequestContent({ content, error, isEncoded }) {
      TestRunner.addResult('Highlighter type: ' + uiSourceCode.mimeType());
      TestRunner.addResult('UISourceCode content: ' + content);
      callback();
    }
  }

  TestRunner.runTestSuite([
    function testDocumentResource(next) {
      TestRunner.addResult('Creating resource.');
      TestRunner.waitForUISourceCode('resources/syntax-error.html')
          .then(uiSourceCodeAdded);
      TestRunner.addIframe('resources/syntax-error.html');

      function uiSourceCodeAdded(uiSourceCode) {
        dumpUISourceCode(uiSourceCode, next);
      }
    },

    function testVMScript(next) {
      TestRunner.addResult('Creating script.');
      TestRunner.evaluateInPageAnonymously(
          'var foo=1;\n//# sourceURL=foo.js\n');
      TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.Network).then(uiSourceCodeAdded);

      function uiSourceCodeAdded(uiSourceCode) {
        dumpUISourceCode(uiSourceCode, next);
      }
    },

    function testScriptResource(next) {
      TestRunner.addResult('Creating script resource.');
      TestRunner.waitForUISourceCode('script1.js', Workspace.projectTypes.Network).then(uiSourceCodeAdded);
      TestRunner.addScriptTag('resources/script1.js');

      function uiSourceCodeAdded(uiSourceCode) {
        dumpUISourceCode(uiSourceCode, next);
      }
    },

    function testRemoveStyleSheetFromModel(next) {
      TestRunner.waitForUISourceCode('style1.css').then(uiSourceCodeAdded);
      TestRunner.addResult('Creating stylesheet resource.');
      TestRunner.addStylesheetTag('resources/style1.css');

      function uiSourceCodeAdded(uiSourceCode) {
        TestRunner.addResult(
            'Added uiSourceCode: ' + uiSourceCodeURL(uiSourceCode));
        TestRunner.waitForUISourceCodeRemoved(uiSourceCodeRemoved);
        TestRunner.evaluateInPage('removeStyleSheet()');
      }

      function uiSourceCodeRemoved(uiSourceCode) {
        TestRunner.addResult(
            'Removed uiSourceCode: ' + uiSourceCodeURL(uiSourceCode));
        next();
      }
    }
  ]);
})();
