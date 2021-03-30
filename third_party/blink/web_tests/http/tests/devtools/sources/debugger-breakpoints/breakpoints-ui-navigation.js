// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests breakpoints on navigation.');
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  await TestRunner.navigate(TestRunner.url('resources/a.html'));

  // Pairs of line number plus breakpoint decoration counts.
  // We expect line 3 to have 2 decorations and line 5 to have one decoration and so on.
  const expectedDecorationsHtml = [[3, 2], [5, 1], [6, 1]];
  const expectedDecorationsJs = [[5, 1], [9, 1], [10, 1]];

  let sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.html');
  TestRunner.addResult('Set different breakpoints in inline script and dump them');
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(sourceFrame, expectedDecorationsHtml, async () => {
    await SourcesTestRunner.toggleBreakpoint(sourceFrame, 3, false);
    await SourcesTestRunner.createNewBreakpoint(sourceFrame, 5, 'a === 3', true);
    await SourcesTestRunner.createNewBreakpoint(sourceFrame, 6, '', false);
  });

  sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.js');
  TestRunner.addResult('Set different breakpoints and dump them');
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(sourceFrame, expectedDecorationsJs, async () => {
    await SourcesTestRunner.toggleBreakpoint(sourceFrame, 9, false);
    await SourcesTestRunner.createNewBreakpoint(sourceFrame, 10, 'a === 3', true);
    await SourcesTestRunner.createNewBreakpoint(sourceFrame, 5, '', false);
  });

  TestRunner.addResult('Dump to b.html and check that there is no breakpoints');
  await TestRunner.navigate(TestRunner.url('resources/b.html'));
  sourceFrame = await SourcesTestRunner.showScriptSourcePromise('b.html');
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(
      sourceFrame, [], () => {}, true);

  TestRunner.addResult('Navigate back to a.html and dump breakpoints');
  await TestRunner.navigate(TestRunner.url('resources/a.html'));
  TestRunner.addResult('a.html:');
  sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.html');
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(
      sourceFrame, expectedDecorationsHtml, () => {}, true);
  TestRunner.addResult('a.js:');
  sourceFrame = await SourcesTestRunner.showScriptSourcePromise('a.js');
  await SourcesTestRunner.runActionAndWaitForExactBreakpointDecorations(
      sourceFrame, expectedDecorationsJs, () => {}, true);

  TestRunner.completeTest();
})();
