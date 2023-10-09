// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Persistence from 'devtools/models/persistence/persistence.js';

(async function() {
  TestRunner.addResult(`Ensures that when a project is added with already existing files they bind.\n`);

  await TestRunner.navigatePromise('http://127.0.0.1:8000/devtools/network/resources/empty.html');

  Persistence.Persistence.PersistenceImpl.instance().addEventListener(Persistence.Persistence.Events.BindingCreated, event => {
    const binding = event.data;
    TestRunner.addResult('Bound Files:');
    TestRunner.addResult(binding.network.url() + ' <=> ' + binding.fileSystem.url());
    TestRunner.addResult('');
    TestRunner.completeTest();
  });

  const { testFileSystem } = await BindingsTestRunner.createOverrideProject('file:///tmp');
  testFileSystem.addFile('127.0.0.1:8000/devtools/network/resources/empty.html', 'New Content');

  BindingsTestRunner.setOverridesEnabled(true);
})();
