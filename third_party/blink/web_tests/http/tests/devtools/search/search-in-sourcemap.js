// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests single resource search in inspector page agent.\n`);
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigate('resources/sourcemap-page.html');

  await Promise.all([
    TestRunner.waitForUISourceCode('sourcemap-script.js'),
    TestRunner.waitForUISourceCode('sourcemap-typescript.ts'),
    TestRunner.waitForUISourceCode('sourcemap-style.css'),
    TestRunner.waitForUISourceCode('sourcemap-sass.scss'),
  ]);
  var scope = new Sources.SourcesSearchScope();

  var query = 'color: blue';
  TestRunner.addResult('\nSearching for: "' + query + '"');
  var searchConfig = new Search.SearchConfig(query, true /* ignoreCase */, false /* isRegex */);
  await new Promise(x => SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, x));

  var query = 'window.foo';
  TestRunner.addResult('\nSearching for: "' + query + '"');
  var searchConfig = new Search.SearchConfig(query, true /* ignoreCase */, false /* isRegex */);
  await new Promise(x => SourcesTestRunner.runSearchAndDumpResults(scope, searchConfig, x));

  TestRunner.completeTest();
})();
