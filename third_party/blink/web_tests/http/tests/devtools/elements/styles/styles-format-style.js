// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that css properties are correctly formatted by the styles sidebar.`);
  await TestRunner.showPanel('elements');
  const tokenizerFactory = await self.runtime.extension(TextUtils.TokenizerFactory).instance();

  testText('color: red;');
  testText('color: red;;;');
  testText('color: red;;;color: blue');
  testText('color: var(-);margin: 0;padding:0');
  testText('color: red;/* a comment */;color: blue');
  testText(`:; color: red; color: blue`);
  testText('color: red;/* a comment;;; */ :; color: blue;');
  testText('grid: "a a" 10px "b b" 20px / 100px');
  testText('grid: [first-row-start] "a a" 10px [first-row-end] [second-row-start] "b b" 20px / 100px');
  TestRunner.completeTest();

  function testText(cssText) {
    TestRunner.addResult(`\nRaw CSS: ${cssText}`);
    const newText = SDK.CSSProperty._formatStyle(cssText, ' ','', tokenizerFactory);
    TestRunner.addResult(`New CSS: ${newText}`);
  }

})();