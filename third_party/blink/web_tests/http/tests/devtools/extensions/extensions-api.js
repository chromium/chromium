// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests public interface of WebInspector Extensions API\n`);
  await TestRunner.loadTestModule('extensions_test_runner');

  await ExtensionsTestRunner.runExtensionTests([
    function extension_testAPI(nextTest) {
      dumpObject(webInspector);
      nextTest();
    }
  ]);
})();
