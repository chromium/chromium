// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Host from 'devtools/core/host/host.js';
import * as Persistence from 'devtools/models/persistence/persistence.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`Tests file system project.\n`);
  await TestRunner.showPanel('sources');

  function fileSystemUISourceCodes() {
    var uiSourceCodes = [];
    var fileSystemProjects = Workspace.Workspace.WorkspaceImpl.instance().projectsForType(Workspace.Workspace.projectTypes.FileSystem);
    for (var project of fileSystemProjects) {
      for (const uiSourceCode of project.uiSourceCodes()) {
        uiSourceCodes.push(uiSourceCode);
      }
    }
    return uiSourceCodes;
  }

  function dumpUISourceCode(uiSourceCode, callback) {
    TestRunner.addResult('UISourceCode: ' + uiSourceCode.url().replace(/.*(LayoutTests|web_tests)./, ''));
    if (uiSourceCode.contentType() === Common.ResourceType.resourceTypes.Script ||
        uiSourceCode.contentType() === Common.ResourceType.resourceTypes.Document)
      TestRunner.addResult(
          'UISourceCode is content script: ' +
          (uiSourceCode.project().type() === Workspace.Workspace.projectTypes.ContentScripts));
    uiSourceCode.requestContent().then(didRequestContent);

    function didRequestContent(content, contentEncoded) {
      TestRunner.addResult('Highlighter type: ' + uiSourceCode.mimeType());
      TestRunner.addResult('UISourceCode content: ' + content.content);
      callback();
    }
  }

  function dumpUISourceCodes(uiSourceCodes, next) {
    innerDumpUISourceCodes(uiSourceCodes, 0, next);

    function innerDumpUISourceCodes(uiSourceCodes, startIndex, next) {
      TestRunner.addResult('');
      if (startIndex === uiSourceCodes.length) {
        next();
        return;
      }

      dumpUISourceCode(
          uiSourceCodes[startIndex], innerDumpUISourceCodes.bind(this, uiSourceCodes, startIndex + 1, next));
    }
  }

  function dumpUISourceCodeLocations(uiSourceCodes, lineNumber) {
    TestRunner.addResult('Dumping uiSourceCode location link texts:');
    for (var i = 0; i < uiSourceCodes.length; ++i) {
      var uiSourceCode = uiSourceCodes[i];
      var uiLocation = uiSourceCode.uiLocation(lineNumber);
      TestRunner.addResult(' - ' + uiLocation.linkText());
    }
  }

  function dumpWorkspaceUISourceCodes() {
    TestRunner.addResult('Dumping uiSourceCodes origin URLs:');
    var uiSourceCodes = fileSystemUISourceCodes();
    for (var i = 0; i < uiSourceCodes.length; ++i)
      TestRunner.addResult('  - ' + uiSourceCodes[i].url());
  }

  function createFileSystem(name, callback) {
    var fs = new BindingsTestRunner.TestFileSystem(name);
    fs.root.mkdir('html').addFile('foo.js', '');
    fs.root.mkdir('.git').addFile('foogit.js', '');
    fs.root.addFile('bar.js', '');
    fs.root.mkdir('html2').addFile('foo.js', '');
    fs.reportCreated(callback.bind(null, fs));
  }

  TestRunner.runTestSuite([
    function testFileSystems(next) {
      TestRunner.addResult('Adding first file system.');
      var fs1 = new BindingsTestRunner.TestFileSystem('/var/www');
      var fs2 = new BindingsTestRunner.TestFileSystem('/foo/bar');
      TestRunner.addResult('Adding second file system.');

      TestRunner.addResult('Adding files to file systems.');

      var localhostDir = fs1.root.mkdir('localhost');
      localhostDir.addFile('foo.js', '<foo content>');
      fs1.root.addFile('bar.js', '<bark content>');

      fs2.root.addFile('baz.js', '<bazzz content>');
      fs1.reportCreated(function() {});
      fs2.reportCreated(function() {});

      Workspace.Workspace.WorkspaceImpl.instance().addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, onUISourceCode);

      var count = 3;
      function onUISourceCode() {
        if (--count)
          return;
        Workspace.Workspace.WorkspaceImpl.instance().removeEventListener(Workspace.Workspace.Events.UISourceCodeAdded, onUISourceCode);
        onUISourceCodesLoaded();
      }

      var uiSourceCodes;

      function onUISourceCodesLoaded() {
        uiSourceCodes = fileSystemUISourceCodes();
        dumpUISourceCodes(uiSourceCodes, uiSourceCodesDumped);
      }

      function uiSourceCodesDumped() {
        dumpUISourceCodeLocations(uiSourceCodes, 5);
        Workspace.Workspace.WorkspaceImpl.instance().addEventListener(Workspace.Workspace.Events.WorkingCopyCommitted, contentCommitted, this);
        uiSourceCodes[0].addRevision('<Modified UISourceCode content>');
      }

      function contentCommitted() {
        TestRunner.addResult('After revision added:');
        dumpUISourceCode(uiSourceCodes[0], finalize);
      }

      function finalize() {
        fs1.reportRemoved();
        fs2.reportRemoved();
        next();
      }
    },

    function testDefaultExcludes(next) {
      createFileSystem('/var/www', dumpExcludes);

      function dumpExcludes(fs) {
        TestRunner.addResult('');
        TestRunner.addResult('-- Default excludes --');
        dumpWorkspaceUISourceCodes();
        fs.reportRemoved();
        next();
      }
    },

    function testExcludesSettings(next) {
      Common.Settings.Settings.instance().createLocalSetting('workspace-excluded-folders', {}).set({'file:///var/www2': ['/html/']});
      createFileSystem('/var/www2', dumpExcludes);

      function dumpExcludes(fs) {
        TestRunner.addResult('');
        TestRunner.addResult('-- Excluded /html/ --');
        dumpWorkspaceUISourceCodes();
        fs.reportRemoved();
        next();
      }
    },

    function testExcludesViaDelegate(next) {
      createFileSystem('/var/www3', dumpExcludes);

      function dumpExcludes(fs) {
        fileSystemUISourceCodes()[0].project().excludeFolder('file:///var/www3/html2/');
        TestRunner.addResult('');
        TestRunner.addResult('-- Excluded /html2/ --');
        dumpWorkspaceUISourceCodes();
        fs.reportRemoved();
        next();
      }
    },

    function testFileAddedExternally(next) {
      var fs = new BindingsTestRunner.TestFileSystem('/var/www4');
      var dir = fs.root.mkdir('html');
      dir.addFile('foo.js', '');
      fs.reportCreated(dumpFileSystem);

      function dumpFileSystem() {
        TestRunner.addResult('-- Original tree --');
        dumpWorkspaceUISourceCodes();

        dir.addFile('bar.js', '');
        Host.InspectorFrontendHost.InspectorFrontendHostInstance.events.dispatchEventToListeners(
            Host.InspectorFrontendHostAPI.Events.FileSystemFilesChangedAddedRemoved,
            {changed: [], added: ['/var/www4/html/bar.js'], removed: []});

        TestRunner.addResult('-- File added externally --');
        dumpWorkspaceUISourceCodes();
        fs.reportRemoved();
        next();
      }
    },

    function testGitFolders(next) {
      var fs = new BindingsTestRunner.TestFileSystem('/var/www3');
      var project1 = fs.root.mkdir('project_1');
      project1.mkdir('.git').addFile('foo.git');
      var project2 = fs.root.mkdir('project_2');
      project2.mkdir('.git').addFile('bar.git');
      var project3 = fs.root.mkdir('project_3');
      project3.mkdir('.svn').addFile('baz.svn');
      var project4 = fs.root.mkdir('project_4');
      project4.addFile('index.html');
      fs.reportCreated(dumpGitFolders);

      function dumpGitFolders() {
        var isolatedFileSystem = Persistence.IsolatedFileSystemManager.IsolatedFileSystemManager.instance().fileSystem('file:///var/www3');
        var folders = isolatedFileSystem.initialGitFolders();
        folders.sort();
        for (var gitFolder of folders)
          TestRunner.addResult(gitFolder);
        fs.reportRemoved();
        next();
      }
    },

    function testUISourceCodeMetadata(next) {
      var fs = new BindingsTestRunner.TestFileSystem('/var/www3');
      var file = fs.root.mkdir('test').addFile('hello.js', '123456');
      fs.reportCreated(function() {});
      SourcesTestRunner.waitForScriptSource('hello.js', onUISourceCode);
      var uiSourceCode;

      function onUISourceCode(sourceCode) {
        uiSourceCode = sourceCode;
        uiSourceCode.requestMetadata().then(onInitialMetadata);
      }

      function onInitialMetadata(metadata) {
        dumpMetadata('Initial metadata', metadata);
        file.setContent('changed content');
        uiSourceCode.requestMetadata().then(onChangedMetadata);
      }

      function onChangedMetadata(metadata) {
        dumpMetadata('Changed metadata', metadata);
        fs.reportRemoved();
        next();
      }

      function dumpMetadata(label, metadata) {
        TestRunner.addResult(label);
        TestRunner.addResult('    content size: ' + metadata.contentSize);
        TestRunner.addResult('    modification time: ' + metadata.modificationTime.toISOString());
      }
    },

    function testFileRename(next) {
      var fs = new BindingsTestRunner.TestFileSystem('/var/www3');
      var file = fs.root.mkdir('test').addFile('hello.js', '123456');
      fs.reportCreated(function() {});
      SourcesTestRunner.waitForScriptSource('hello.js', onUISourceCode);
      var uiSourceCode;
      var originalURL;
      function onUISourceCode(sourceCode) {
        uiSourceCode = sourceCode;
        originalURL = uiSourceCode.url();
        TestRunner.addResult('URL before rename: ' + originalURL);
        uiSourceCode.rename('goodbye.js').then(renamed);
      }

      function renamed() {
        TestRunner.addResult('URL after rename: ' + uiSourceCode.url());
        if (uiSourceCode.project().workspace().uiSourceCodeForURL(originalURL))
          TestRunner.addResult('ERROR: Still found original URL in workspace.');
        if (!uiSourceCode.project().workspace().uiSourceCodeForURL(uiSourceCode.url()))
          TestRunner.addResult('ERROR: Could not find new URL in workspace.');
        next();
      }
    }
  ]);
})();
