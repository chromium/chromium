// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

(async function() {
  TestRunner.addResult(`This test checks HeapSnapshots loader.\n`);
  await TestRunner.showPanel('heap_profiler');

  var source = HeapProfilerTestRunner.createHeapSnapshotMockRaw();
  var sourceStringified = JSON.stringify(source);
  var partSize = sourceStringified.length >> 3;

  async function injectMockProfile(callback) {
    var dispatcher = TestRunner.mainTarget._dispatchers['HeapProfiler']._dispatchers[0];
    var panel = UI.panels.heap_profiler;
    panel._reset();

    var profileType = Profiler.ProfileTypeRegistry.instance.heapSnapshotProfileType;

    TestRunner.override(TestRunner.HeapProfilerAgent, 'invoke_takeHeapSnapshot', takeHeapSnapshotMock);
    function takeHeapSnapshotMock(reportProgress) {
      if (reportProgress) {
        profileType._reportHeapSnapshotProgress({data: {done: 50, total: 100, finished: false}});
        profileType._reportHeapSnapshotProgress({data: {done: 100, total: 100, finished: true}});
      }
      for (var i = 0, l = sourceStringified.length; i < l; i += partSize)
        dispatcher.addHeapSnapshotChunk(sourceStringified.slice(i, i + partSize));
      return Promise.resolve();
    }

    function tempFileReady() {
      callback(this);
    }
    TestRunner.addSniffer(Profiler.HeapProfileHeader.prototype, '_didWriteToTempFile', tempFileReady);
    if (!UI.context.flavor(SDK.HeapProfilerModel))
      await new Promise(resolve => UI.context.addFlavorChangeListener(SDK.HeapProfilerModel, resolve));
    profileType._takeHeapSnapshot();
  }

  Common.console.log = function(message) {
    TestRunner.addResult('SDK.consoleModel.log: ' + message);
  };

  TestRunner.runTestSuite([
    function heapSnapshotSaveToFileTest(next) {
      function snapshotLoaded(profileHeader) {
        var savedSnapshotData;
        function saveMock(url, data) {
          savedSnapshotData = data;
          setTimeout(() => Workspace.fileManager._savedURL({data: {url: url}}), 0);
        }
        TestRunner.override(InspectorFrontendHost, 'save', saveMock);

        var oldAppend = InspectorFrontendHost.append;
        InspectorFrontendHost.append = function appendMock(url, data) {
          savedSnapshotData += data;
          Workspace.fileManager._appendedToURL({data: url});
        };
        function closeMock(url) {
          TestRunner.assertEquals(sourceStringified, savedSnapshotData, 'Saved snapshot data');
          InspectorFrontendHost.append = oldAppend;
          next();
        }
        TestRunner.override(Workspace.FileManager.prototype, 'close', closeMock);
        profileHeader.saveToFile();
      }

      injectMockProfile(snapshotLoaded);
    },

    function heapSnapshotLoadFromFileTest(next) {
      var panel = UI.panels.heap_profiler;

      var fileMock = {name: 'mock.heapsnapshot', size: sourceStringified.length};

      Bindings.ChunkedFileReader = class {
        constructor() {
        }

        read(receiver) {
          receiver.write(sourceStringified);
          receiver.close();
          return Promise.resolve(true);
        }

        loadedSize() {
          return fileMock.size;
        }

        fileSize() {
          return fileMock.size;
        }

        fileName() {
          return fileMock.name;
        }
      };
      TestRunner.addSniffer(Profiler.HeapProfileHeader.prototype, '_snapshotReceived', next);
      panel._loadFromFile(fileMock);
    },

    function heapSnapshotRejectToSaveToFileTest(next) {
      function snapshotLoaded(profileHeader) {
        if (profileHeader.canSaveToFile())
          next();
        else
          profileHeader.addEventListener(Profiler.ProfileHeader.Events.ProfileReceived, onCanSaveProfile, this);
        function onCanSaveProfile() {
          TestRunner.assertTrue(profileHeader.canSaveToFile());
          next();
        }
      }

      injectMockProfile(snapshotLoaded);
    }
  ]);
})();
