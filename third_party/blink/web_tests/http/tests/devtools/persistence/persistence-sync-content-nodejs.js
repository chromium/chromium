// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Persistence from 'devtools/models/persistence/persistence.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Verify that syncing Node.js contents works fine.\n`);

  var testMapping = BindingsTestRunner.initializeTestMapping();
  // Pretend we are running under V8 front-end.
  SDK.TargetManager.TargetManager.instance().primaryPageTarget().markAsNodeJSForTest();

  var content = ['', '', 'var express = require("express");', '//TODO'].join('\n');

  var fsContent = Persistence.Persistence.NodeShebang + content;
  var nodeContent = Persistence.Persistence.NodePrefix + content + Persistence.Persistence.NodeSuffix;

  TestRunner.addResult('Initial fileSystem content:');
  TestRunner.addResult(indent(fsContent));
  TestRunner.addResult('\n Initial network content:');
  TestRunner.addResult(indent(nodeContent));

  await SourcesTestRunner.addScriptUISourceCode('http://127.0.0.1:8000/nodejs.js', nodeContent);

  // Add filesystem UISourceCode and mapping.
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  var fsEntry = fs.root.addFile('nodejs.js', fsContent);
  fs.reportCreated(function() {});

  var binding;
  testMapping.addBinding('nodejs.js');
  BindingsTestRunner.waitForBinding('nodejs.js').then(onBindingCreated);

  function onBindingCreated(theBinding) {
    binding = theBinding;
    TestRunner.addResult('Binding created: ' + binding);
    TestRunner.runTestSuite(testSuite);
  }

  var testSuite = [
    function addNetworkUISourceCodeRevision(next) {
      var newContent = nodeContent.replace('//TODO', 'network();\n//TODO');
      TestRunner.addSniffer(Persistence.Persistence.PersistenceImpl.prototype, 'contentSyncedForTest', onSynced);
      const writePromise = TestRunner.addSnifferPromise(BindingsTestRunner.TestFileSystem.Writer.prototype, 'truncate');
      binding.network.addRevision(newContent);

      function onSynced() {
        dumpBindingContent();
        writePromise.then(next);
      }
    },

    function setNetworkUISourceCodeWorkingCopy(next) {
      var newContent = nodeContent.replace('//TODO', 'workingCopy1();\n//TODO');
      TestRunner.addSniffer(Persistence.Persistence.PersistenceImpl.prototype, 'contentSyncedForTest', onSynced);
      binding.network.setWorkingCopy(newContent);

      function onSynced() {
        dumpBindingContent();
        next();
      }
    },

    async function changeFileSystemFile(next) {
      var newContent = fsContent.replace('//TODO', 'filesystem();\n//TODO');
      TestRunner.addSniffer(Persistence.Persistence.PersistenceImpl.prototype, 'contentSyncedForTest', onSynced);
      fsEntry.setContent(newContent);

      function onSynced() {
        dumpBindingContent();
        next();
      }
    },

    function setFileSystemUISourceCodeWorkingCopy(next) {
      var newContent = fsContent.replace('//TODO', 'workingCopy2();\n//TODO');
      TestRunner.addSniffer(Persistence.Persistence.PersistenceImpl.prototype, 'contentSyncedForTest', onSynced);
      binding.fileSystem.setWorkingCopy(newContent);

      function onSynced() {
        dumpBindingContent();
        next();
      }
    },
  ];

  function dumpBindingContent() {
    TestRunner.addResult('Network:');
    TestRunner.addResult(indent(binding.network.workingCopy()));
    TestRunner.addResult('');
    TestRunner.addResult('FileSystem:');
    TestRunner.addResult(indent(binding.fileSystem.workingCopy()));
    TestRunner.addResult('');
  }

  function indent(content) {
    return content.split('\n').map(line => '    ' + line).join('\n');
  }
})();
