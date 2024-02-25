// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Persistence from 'devtools/models/persistence/persistence.js';

(async function() {
  TestRunner.addResult(`Tests that persistence syncs network and filesystem UISourceCodes.\n`);
  await TestRunner.addScriptTag('resources/foo.js');

  var testMapping = BindingsTestRunner.initializeTestMapping();
  var fs = new BindingsTestRunner.TestFileSystem('/var/www');
  var fsEntry = BindingsTestRunner.addFooJSFile(fs);

  var networkCode, fileSystemCode;

  TestRunner.runTestSuite([
    function addFileSystem(next) {
      fs.reportCreated(next);
    },

    function addFileMapping(next) {
      testMapping.addBinding('foo.js');
      BindingsTestRunner.waitForBinding('foo.js').then(onBindingCreated);

      function onBindingCreated(binding) {
        TestRunner.addResult('Binding created: ' + binding);
        networkCode = binding.network;
        fileSystemCode = binding.fileSystem;
        next();
      }
    },

    function addFileSystemRevision(next) {
      TestRunner.addSniffer(
          Persistence.Persistence.PersistenceImpl.prototype, 'contentSyncedForTest', dumpWorkingCopiesAndNext.bind(null, next));
      fsEntry.setContent('window.foo3 = 3;');
    },

    function addFileSystemWorkingCopy(next) {
      TestRunner.addSniffer(
          Persistence.Persistence.PersistenceImpl.prototype, 'contentSyncedForTest', dumpWorkingCopiesAndNext.bind(null, next));
      fileSystemCode.setWorkingCopy('window.foo4 = 4;');
    },

    function resetFileSystemWorkingCopy(next) {
      TestRunner.addSniffer(
          Persistence.Persistence.PersistenceImpl.prototype, 'contentSyncedForTest', dumpWorkingCopiesAndNext.bind(null, next));
      fileSystemCode.resetWorkingCopy();
    },

    function setNetworkRevision(next) {
      TestRunner.addSniffer(
          Persistence.Persistence.PersistenceImpl.prototype, 'contentSyncedForTest', dumpWorkingCopiesAndNext.bind(null, next));
      networkCode.addRevision('window.foo2 = 2;');
    },

    function setNetworkWorkingCopy(next) {
      TestRunner.addSniffer(
          Persistence.Persistence.PersistenceImpl.prototype, 'contentSyncedForTest', dumpWorkingCopiesAndNext.bind(null, next));
      networkCode.setWorkingCopy('window.foo5 = 5;');
    },
  ]);

  function dumpWorkingCopiesAndNext(next) {
    TestRunner.addResult(`network code: '${networkCode.workingCopy()}'`);
    TestRunner.addResult(`fileSystem code: '${fileSystemCode.workingCopy()}'`);
    next();
  }
})();
