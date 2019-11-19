// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests SourceMap and StyleSheetMapping.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.evaluateInPagePromise(`
      function addStyleSheet()
      {
          var style = document.createElement("link");
          style.setAttribute("rel", "stylesheet");
          style.setAttribute("type", "text/css");
          style.setAttribute("href", "http://127.0.0.1:8000/devtools/resources/example.css");
          document.head.appendChild(style);
      }
  `);

  var contentReceived;
  var finalMappedLocation;
  var target = TestRunner.mainTarget;
  var cssModel = TestRunner.cssModel;

  const styleSheetURL = 'http://127.0.0.1:8000/devtools/resources/example.css';
  const sourceURL = 'http://127.0.0.1:8000/devtools/resources/example.scss';
  var styleSheetId;

  TestRunner.waitForUISourceCode(styleSheetURL).then(cssUISourceCodeAdded);
  TestRunner.evaluateInPage('addStyleSheet()');

  function locationsUpdated() {
    var header = cssModel.styleSheetHeaderForId(styleSheetId);
    var uiLocation = Bindings.cssWorkspaceBinding.rawLocationToUILocation(new SDK.CSSLocation(header, 2, 3));
    if (uiLocation.uiSourceCode.url().indexOf('.scss') === -1)
      return;
    finalMappedLocation = uiLocation.uiSourceCode.url() + ':' + uiLocation.lineNumber + ':' + uiLocation.columnNumber;
    join();
  }

  function cssUISourceCodeAdded(uiSourceCode) {
    styleSheetId = cssModel.styleSheetIdsForURL(styleSheetURL)[0];
    TestRunner.addSniffer(Bindings.CSSWorkspaceBinding.ModelInfo.prototype, '_updateLocations', locationsUpdated, true);
    TestRunner.addResult('Added CSS uiSourceCode: ' + uiSourceCode.url());
    TestRunner.waitForUISourceCode(sourceURL).then(scssUISourceCodeAdded);
  }

  function testAndDumpLocation(uiSourceCode, expectedLine, expectedColumn, line, column) {
    var header = cssModel.styleSheetHeaderForId(styleSheetId);
    var uiLocation = Bindings.cssWorkspaceBinding.rawLocationToUILocation(new SDK.CSSLocation(header, line, column));
    TestRunner.assertEquals(
        uiSourceCode, uiLocation.uiSourceCode,
        `Incorrect uiSourceCode, expected ${uiSourceCode.url()}, but got ${
            location.uiSourceCode ? location.uiSourceCode.url() : null}`);
    var reverseRaw = Bindings.cssWorkspaceBinding.uiLocationToRawLocations(uiLocation)[0];
    TestRunner.addResult(
        `${line}:${column} ${uiLocation.lineNumber}:${uiLocation.columnNumber}` +
        `(expected: ${expectedLine}:${expectedColumn}) -> ${reverseRaw.lineNumber}:${reverseRaw.columnNumber}`);
  }

  function scssUISourceCodeAdded(uiSourceCode) {
    TestRunner.addResult('Added SCSS uiSourceCode: ' + uiSourceCode.url());
    var cssUISourceCode = Workspace.workspace.uiSourceCodeForURL(styleSheetURL);
    var scssUISourceCode = Workspace.workspace.uiSourceCodeForURL(sourceURL);

    testAndDumpLocation(cssUISourceCode, 0, 3, 0, 3);
    testAndDumpLocation(scssUISourceCode, 1, 0, 1, 0);
    testAndDumpLocation(scssUISourceCode, 2, 2, 2, 4);
    testAndDumpLocation(scssUISourceCode, 2, 5, 2, 6);
    testAndDumpLocation(scssUISourceCode, 2, 7, 2, 9);
    testAndDumpLocation(scssUISourceCode, 2, 10, 3, 7);
    testAndDumpLocation(scssUISourceCode, 4, 2, 4, 8);
    testAndDumpLocation(scssUISourceCode, 4, 2, 4, 10);
    testAndDumpLocation(scssUISourceCode, 4, 11, 4, 11);
    testAndDumpLocation(scssUISourceCode, 4, 13, 4, 15);
    testAndDumpLocation(scssUISourceCode, 4, 17, 4, 20);
    scssUISourceCode.requestContent().then(didRequestContent);

    function didRequestContent({ content, error, isEncoded }) {
      TestRunner.assertEquals(0, content.indexOf('/* Comment */'));
      contentReceived = true;
      join();
    }
  }

  function join() {
    if (!contentReceived || !finalMappedLocation)
      return;
    TestRunner.addResult('UILocation upon LiveLocation update: ' + finalMappedLocation);
    TestRunner.completeTest();
  }
})();
