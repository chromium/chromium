// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Verify that messages are synced in UISourceCodeFrame between UISourceCodes of persistence binding.\n`);
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('resources/foo.js');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('file:///var/www');
  BindingsTestRunner.addFooJSFile(fs);
  var networkSourceCode;
  var fileSystemSourceCode;
  var fileSystemSourceFrame, networkSourceFrame;

  TestRunner.runTestSuite([
    function waitForUISourceCodes(next) {
      fs.reportCreated(function() {});
      Promise
          .all([
            TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.FileSystem)
                .then(sourceCode => fileSystemSourceCode = sourceCode),
            TestRunner.waitForUISourceCode('foo.js', Workspace.projectTypes.Network)
                .then(sourceCode => networkSourceCode = sourceCode),
          ])
          .then(next);
    },

    function addMessages(next) {
      fileSystemSourceCode.addLineMessage(Workspace.UISourceCode.Message.Level.Error, 'error in filesystem', 0, 0);
      networkSourceCode.addLineMessage(Workspace.UISourceCode.Message.Level.Warning, 'warning in network', 1, 0);

      Promise
          .all([
            SourcesTestRunner.showUISourceCodePromise(fileSystemSourceCode),
            SourcesTestRunner.showUISourceCodePromise(networkSourceCode)
          ])
          .then(onSourceFrames);

      function onSourceFrames(sourceFrames) {
        fileSystemSourceFrame = sourceFrames[0];
        networkSourceFrame = sourceFrames[1];
        SourcesTestRunner.dumpSourceFrameMessages(fileSystemSourceFrame, /* dumpFullURL */ true);
        SourcesTestRunner.dumpSourceFrameMessages(networkSourceFrame, /* dumpFullURL */ true);
        next();
      }
    },

    function addMapping(next) {
      testMapping.addBinding('foo.js');
      BindingsTestRunner.waitForBinding('foo.js').then(onBindingCreated);

      function onBindingCreated(binding) {
        SourcesTestRunner.dumpSourceFrameMessages(fileSystemSourceFrame, /* dumpFullURL */ true);
        SourcesTestRunner.dumpSourceFrameMessages(networkSourceFrame, /* dumpFullURL */ true);
        next();
      }
    },

    function removeMapping(next) {
      Persistence.persistence.addEventListener(Persistence.Persistence.Events.BindingRemoved, onBindingRemoved);
      testMapping.removeBinding('foo.js');

      function onBindingRemoved(event) {
        var binding = event.data;
        if (binding.network.name() !== 'foo.js')
          return;
        Persistence.persistence.removeEventListener(Persistence.Persistence.Events.BindingRemoved, onBindingRemoved);
        SourcesTestRunner.dumpSourceFrameMessages(fileSystemSourceFrame, /* dumpFullURL */ true);
        SourcesTestRunner.dumpSourceFrameMessages(networkSourceFrame, /* dumpFullURL */ true);
        next();
      }
    },
  ]);
})();
