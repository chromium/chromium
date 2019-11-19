// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that Source Frame can pretty print\n`);
  await TestRunner.loadModule('source_frame');
  var sourceFrame = new SourceFrame.SourceFrame(async function() {
    return {
      content: `var theContent = something; if (thisIsOnSameLine) { itShouldBeMovedToAnotherLine(); } thenPretty();`,
      error: null,
      isEncoded: false,
    };
  });
  sourceFrame.setHighlighterType('text/javascript');
  sourceFrame.setCanPrettyPrint(true);

  await Promise.all([
    TestRunner.addSnifferPromise(sourceFrame, 'setContent'),
    sourceFrame.show(UI.inspectorView.element)]);
  TestRunner.addResult('Showing raw content: ' + !sourceFrame._prettyToggle.toggled());
  TestRunner.addResult(sourceFrame.textEditor.text());
  TestRunner.addResult('');

  await Promise.all([
      TestRunner.addSnifferPromise(sourceFrame, 'setContent'),
      sourceFrame._prettyToggle.element.click()]);
  TestRunner.addResult('Showing pretty content: ' + sourceFrame._prettyToggle.toggled());
  TestRunner.addResult(sourceFrame.textEditor.text());
  TestRunner.addResult('');

  await Promise.all([
    TestRunner.addSnifferPromise(sourceFrame, 'setContent'),
    sourceFrame._prettyToggle.element.click()]);
  TestRunner.addResult('Back to raw content: ' + !sourceFrame._prettyToggle.toggled());
  TestRunner.addResult(sourceFrame.textEditor.text());

  TestRunner.completeTest();
})();
