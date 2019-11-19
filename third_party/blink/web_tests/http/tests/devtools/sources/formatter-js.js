// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests how SourceFormatter handles JS sources\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('debugger/resources/obfuscated.js');

  var uiSourceCode = await TestRunner.waitForUISourceCode('obfuscated.js');
  var formatData = await Sources.sourceFormatter.format(uiSourceCode);
  var targetContent = (await formatData.formattedSourceCode.requestContent()).content;

  TestRunner.addResult(`Formatted:\n${targetContent}`);

  var originalContent = (await uiSourceCode.requestContent()).content;
  var text = new TextUtils.Text(originalContent);
  var positions = [];
  for (var offset = originalContent.indexOf('{'); offset >= 0; offset = originalContent.indexOf('{', offset + 1))
    positions.push(text.positionFromOffset(offset));
  var script = Bindings.debuggerWorkspaceBinding.uiLocationToRawLocations(uiSourceCode, 0, 0)[0].script();

  TestRunner.addResult('Location mapping with formatted source:');
  dumpLocations(positions);

  Sources.sourceFormatter.discardFormattedUISourceCode(formatData.formattedSourceCode);

  TestRunner.addResult('Location mapping without formatted source:');
  dumpLocations(positions);

  TestRunner.completeTest();

  function dumpLocations(positions) {
    for (var position of positions) {
      var rawLocation = TestRunner.debuggerModel.createRawLocation(script, position.lineNumber, position.columnNumber);
      var uiLocation = Bindings.debuggerWorkspaceBinding.rawLocationToUILocation(rawLocation);
      var reverseRawLocation = Bindings.debuggerWorkspaceBinding.uiLocationToRawLocations(
          uiLocation.uiSourceCode, uiLocation.lineNumber, uiLocation.columnNumber)[0];
      TestRunner.addResult(
          `${rawLocation.lineNumber}:${rawLocation.columnNumber} -> ${uiLocation.lineNumber}:${
              uiLocation.columnNumber}` +
          ` -> ${reverseRawLocation.lineNumber}:${reverseRawLocation.columnNumber}`);
    }
  }
})();
