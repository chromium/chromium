// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Host from 'devtools/core/host/host.js';
import * as Persistence from 'devtools/models/persistence/persistence.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Ensures iframes are overridable if overrides are setup.\n`);

  var fileSystemPath = 'file:///tmp/';

  var {isolatedFileSystem, project} = await createFileSystem();
  // Using data url because about:blank does not trigger onload.
  await TestRunner.addIframe('data:,', { id: 'test-iframe' });

  Host.Platform.setPlatformForTests('linux');
  await testFileName('resources/bar.js');
  await testFileName('resources/a space/bar.js');
  await testFileName('resources/bar.js?#hello');
  await testFileName('resources/bar.js?params');
  await testFileName('resources/bar.js?params&and=more&pa&ra?ms');
  await testFileName('resources/bar2.js?params&and=more&pa&ra?ms#hello?with&params');
  await testFileName('resources/no-extension');
  await testFileName('resources/foo&with%20some*bad%5EC!h%7Ba...r,acter%%s/file&with?s%20t^rang@e~character%27S');
  await testFileName('resources/'); // Should be index.html

  Host.Platform.setPlatformForTests('windows');
  await testFileName('windows/bar.js');
  await testFileName('windows/a space/bar.js');
  await testFileName('windows/bar.js?#hello');
  await testFileName('windows/bar.js?params');
  await testFileName('windows/bar.js?params&and=more&pa&ra?ms');
  await testFileName('windows/bar2.js?params&and=more&pa&ra?ms#hello?with&params');
  await testFileName('windows/no-extension');
  await testFileName('windows/foo&with%20some*bad%5EC!h%7Ba...r,acter%%s/file&with?s%20t^rang@e~character%27S');
  await testFileName('windows/'); // Should be index.html

  TestRunner.completeTest();

  async function testFileName(url) {
    TestRunner.addResult('Creating UISourcecode for url: ' + url);
    TestRunner.evaluateInPagePromise(`document.getElementById('test-iframe').src = '${url}'`)
    var networkUISourceCode = await TestRunner.waitForEvent(
        Workspace.Workspace.Events.UISourceCodeAdded, Workspace.Workspace.WorkspaceImpl.instance(),
        uiSourceCode => uiSourceCode.url().startsWith('http'));
    if (!networkUISourceCode) {
      TestRunner.addResult('ERROR: No uiSourceCode');
      TestRunner.completeTest();
      return;
    }
    TestRunner.addResult('Found network UISourceCode: ' + networkUISourceCode.url());

    TestRunner.addResult('Saving network UISourceCode');
    Persistence.NetworkPersistenceManager.NetworkPersistenceManager.instance().saveUISourceCodeForOverrides(networkUISourceCode);
    var newFile = await waitForNextCreatedFile();
    TestRunner.addResult('Created File: ' + newFile);
    TestRunner.addResult('');
  }

  async function waitForNextCreatedFile() {
    return new Promise(result => {
      TestRunner.addSniffer(
          Persistence.NetworkPersistenceManager.NetworkPersistenceManager.instance(), 'fileCreatedForTest',
          (path, name) => result(path + '/' + name), false);
    });
  }

  async function createFileSystem() {
    var {isolatedFileSystem, project} = await BindingsTestRunner.createOverrideProject(fileSystemPath);
    BindingsTestRunner.setOverridesEnabled(true);
    return {isolatedFileSystem, project};
  }
})();
