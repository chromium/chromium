// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that an error loading a source-map-referred file will display an error message in the source panel.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('./resources/sourcemap-src-not-loaded.js');

  const jsSource = await TestRunner.waitForUISourceCode('sourcemap-src-not-loaded.js');
  const tsSource = await TestRunner.waitForUISourceCode('sourcemap-src-not-loaded.ts');
  const [jsContent, tsContent] = await Promise.all([
    jsSource.requestContent(),
    tsSource.requestContent(),
  ]);

  TestRunner.addResult('JavaScript source file:');
  TestRunner.addResult(jsContent.content);
  TestRunner.addResult('TypeScript source file:');
  TestRunner.addResult(tsContent.content);
  TestRunner.addResult('TypeScript resolution error:');
  TestRunner.addResult(tsContent.error);

  TestRunner.completeTest();
})();
