// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that different types of inline styles are correctly disambiguated and their sourceURL is correct.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('resources/dynamic-style-tag.html');

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  async function step1() {
    const styleSheets = TestRunner.cssModel.allStyleSheets();
    const styleSheetsWithContent = [];
    for (const header of styleSheets) {
      styleSheetsWithContent.push({
        header,
        content: await TestRunner.CSSAgent.getStyleSheetText(header.id),
      });
    }
    styleSheetsWithContent.sort((a, b) => a.content.localeCompare(b.content));
    for (const {header, content} of styleSheetsWithContent) {
      TestRunner.addResult('Stylesheet added:');
      TestRunner.addResult('  - isInline: ' + header.isInline);
      TestRunner.addResult('  - sourceURL: ' + header.sourceURL.substring(header.sourceURL.lastIndexOf('/') + 1));
      TestRunner.addResult('  - hasSourceURL: ' + header.hasSourceURL);
      TestRunner.addResult('  - contents: ' + content);
    }
    await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
    TestRunner.completeTest();
  }
})();
