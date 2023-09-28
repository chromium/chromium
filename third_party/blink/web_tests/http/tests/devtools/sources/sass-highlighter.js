// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Verify that highlighter type for SCSS file loaded via sourceMap is correct.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.addStylesheetTag('resources/empty.css');

  SourcesTestRunner.showScriptSource('empty.scss', onSourceShown);
  function onSourceShown(uiSourceCodeFrame) {
    var uiSourceCode = uiSourceCodeFrame.uiSourceCode();
    TestRunner.addResult(uiSourceCode.mimeType());
    TestRunner.completeTest();
  }
})();
