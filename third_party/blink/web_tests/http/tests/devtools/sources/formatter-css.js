// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests how SourceFormatter handles CSS sources\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('resources/style-formatter-obfuscated.css');

  var uiSourceCode = await TestRunner.waitForUISourceCode('style-formatter-obfuscated.css');
  var formatData = await Sources.sourceFormatter.format(uiSourceCode);
  var targetContent = (await formatData.formattedSourceCode.requestContent()).content;

  TestRunner.addResult(`Formatted:\n${targetContent}`);

  var originalContent = (await uiSourceCode.requestContent()).content;
  var styleHeader = Bindings.cssWorkspaceBinding.uiLocationToRawLocations(uiSourceCode.uiLocation(0, 0))[0].header();
  var text = new TextUtils.Text(originalContent);
  var liveLocationsPool = new Bindings.LiveLocationPool();
  var locationUpdateCount = 0;
  var rawLocations = [];
  var liveLocations = [];
  for (var offset = originalContent.indexOf('{'); offset >= 0; offset = originalContent.indexOf('{', offset + 1)) {
    var position = text.positionFromOffset(offset);
    var rawLocation = new SDK.CSSLocation(styleHeader, position.lineNumber, position.columnNumber);
    rawLocations.push(rawLocation);
    liveLocations.push(Bindings.cssWorkspaceBinding.createLiveLocation(rawLocation, () => {
      locationUpdateCount++;
    }, liveLocationsPool));
  }

  TestRunner.addResult('Location mapping with formatted source:');
  dumpLocations();

  Sources.sourceFormatter.discardFormattedUISourceCode(formatData.formattedSourceCode);

  TestRunner.addResult('Location mapping without formatted source:');
  dumpLocations();

  TestRunner.completeTest();

  function dumpLocations() {
    TestRunner.addResult('Mapped locations:');
    for (var rawLocation of rawLocations) {
      var uiLocation = Bindings.cssWorkspaceBinding.rawLocationToUILocation(rawLocation);
      var reverseRawLocation = Bindings.cssWorkspaceBinding.uiLocationToRawLocations(uiLocation)[0];
      TestRunner.addResult(
          `${rawLocation.lineNumber}:${rawLocation.columnNumber} -> ${uiLocation.lineNumber}:${
              uiLocation.columnNumber} ` +
          `-> ${reverseRawLocation.lineNumber}:${reverseRawLocation.columnNumber}`);
    }
    TestRunner.addResult(`Live locations (updated: ${locationUpdateCount}):`);
    for (var liveLocation of liveLocations) {
      var uiLocation = liveLocation.uiLocation();
      TestRunner.addResult(`${uiLocation.lineNumber}:${uiLocation.columnNumber}`);
    }
  }
})();
