// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SDK from 'devtools/core/sdk/sdk.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Profiler from 'devtools/panels/profiler/profiler.js';
import * as Workspace from 'devtools/models/workspace/workspace.js';

(async function() {
  TestRunner.addResult(`This test checks HeapSnapshots loader.\n`);
  await TestRunner.showPanel('heap-profiler');

  var source = HeapProfilerTestRunner.createHeapSnapshotMockRaw();
  var sourceStringified = JSON.stringify(source);
  var partSize = sourceStringified.length >> 3;

  async function injectMockProfile(callback) {
    var heapProfilerModel = TestRunner.mainTarget.model(SDK.HeapProfilerModel.HeapProfilerModel);
    var panel = Profiler.HeapProfilerPanel.HeapProfilerPanel.instance();
    panel.reset();

    var profileType = Profiler.ProfileTypeRegistry.instance.heapSnapshotProfileType;

    TestRunner.override(TestRunner.HeapProfilerAgent, 'invoke_takeHeapSnapshot', takeHeapSnapshotMock);
    function takeHeapSnapshotMock(reportProgress) {
      if (reportProgress) {
        profileType.reportHeapSnapshotProgress(
            {data: {done: 50, total: 100, finished: false}});
        profileType.reportHeapSnapshotProgress(
            {data: {done: 100, total: 100, finished: true}});
      }
      for (var i = 0, l = sourceStringified.length; i < l; i += partSize) {
        heapProfilerModel.addHeapSnapshotChunk(
            sourceStringified.slice(i, i + partSize));
      }
      return Promise.resolve();
    }

    function tempFileReady() {
      callback(this);
    }
    TestRunner.addSniffer(
        Profiler.HeapSnapshotView.HeapProfileHeader.prototype, 'didWriteToTempFile',
        tempFileReady);
    if (!UI.Context.Context.instance().flavor(SDK.HeapProfilerModel.HeapProfilerModel)) {
      await new Promise(resolve => UI.Context.Context.instance().addFlavorChangeListener(SDK.HeapProfilerModel.HeapProfilerModel, resolve));
    }
    profileType.takeHeapSnapshot();
  }

  Common.Console.Console.instance().log = function(message) {
    TestRunner.addResult('SDK.consoleModel.log: ' + message);
  };

  TestRunner.runTestSuite([
    function heapSnapshotSaveToFileTest(next) {
      function snapshotLoaded(profileHeader) {
        var savedSnapshotData;
        function saveMock(url, data) {
          savedSnapshotData = data;
          setTimeout(
              () => Workspace.FileManager.FileManager.instance().savedURL({data: {url: url}}), 0);
        }
        TestRunner.override(InspectorFrontendHost, 'save', saveMock);

        var oldAppend = InspectorFrontendHost.append;
        InspectorFrontendHost.append = function appendMock(url, data) {
          savedSnapshotData += data;
          Workspace.FileManager.FileManager.instance().appendedToURL({data: url});
        };
        function closeMock(url) {
          TestRunner.assertEquals(sourceStringified, savedSnapshotData, 'Saved snapshot data');
          InspectorFrontendHost.append = oldAppend;
          next();
        }
        TestRunner.override(Workspace.FileManager.FileManager.prototype, 'close', closeMock);
        profileHeader.saveToFile();
      }

      injectMockProfile(snapshotLoaded);
    },

    function heapSnapshotLoadFromFileTest(next) {
      var panel = Profiler.HeapProfilerPanel.HeapProfilerPanel.instance();
      var file = new File(
          [sourceStringified], 'mock.heapsnapshot', {type: 'text/plain'});
      TestRunner.addSniffer(
          Profiler.HeapSnapshotView.HeapProfileHeader.prototype, 'snapshotReceived', next);
      panel.loadFromFile(file);
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
