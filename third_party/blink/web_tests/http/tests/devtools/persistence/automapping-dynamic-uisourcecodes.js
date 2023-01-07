// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that automapping works property when UISourceCodes come and go.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');

  var foo_js = {content: 'console.log(\'foo.js!\');', time: new Date('December 1, 1989')};

  var automappingTest = new BindingsTestRunner.AutomappingTest(new Workspace.Workspace());
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  fs.reportCreated(onFileSystemCreated);

  function onFileSystemCreated() {
    TestRunner.runTestSuite(tests);
  }

  var tests = [
    function addNetworkResource(next) {
      automappingTest.addNetworkResources({
        'http://example.com/path/foo.js': foo_js,
      });
      automappingTest.waitUntilMappingIsStabilized().then(next);
    },

    function addFileSystemUISourceCode(next) {
      BindingsTestRunner.addFiles(fs, {
        'scripts/foo.js': foo_js,
      });
      automappingTest.waitUntilMappingIsStabilized().then(next);
    },

    function removeNetworkUISourceCode(next) {
      automappingTest.removeResources(['http://example.com/path/foo.js']);
      automappingTest.waitUntilMappingIsStabilized().then(next);
    },

    function reAddNetworkUISourceCode(next) {
      automappingTest.addNetworkResources({
        // Make sure simple resource gets mapped.
        'http://example.com/path/foo.js': foo_js,
      });
      automappingTest.waitUntilMappingIsStabilized().then(next);
    },

    function removeFileSystem(next) {
      fs.reportRemoved();
      automappingTest.waitUntilMappingIsStabilized().then(next);
    },
  ];
})();
