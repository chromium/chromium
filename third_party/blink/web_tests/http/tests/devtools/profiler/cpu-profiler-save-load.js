// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that CPU profiling is able to save/load.\n`);
  await TestRunner.loadTestModule('cpu_profiler_test_runner');
  await TestRunner.showPanel('js_profiler');
  await TestRunner.evaluateInPagePromise(`
      function pageFunction() {
          console.profile("manual");
          console.profileEnd("manual");
      }
  `);

  class MockedFile {
    constructor() {
      this._buffer = '';
    }

    _appendData(data) {
      this._buffer += data;
    }

    _data() {
      return this._buffer;
    }

    get size() {
      return this._buffer.length;
    }

    slice(chunkStart, chunkEnd) {
      var blob = new Blob([this._buffer], {type: 'text\/text'});
      return blob;
    }
  };
  var file = new MockedFile();

  CPUProfilerTestRunner.runProfilerTestSuite([
    function testSave(next) {
      function saveProfileToFile(profile) {
        Bindings.FileOutputStream = function() {};
        Bindings.FileOutputStream.prototype = {
          open: async function(fileName) {
            file.name = fileName;
            if (fileName.endsWith('.cpuprofile')) {
              TestRunner.addResult('PASS: open was called with extension \'.cpuprofile\'');
              return true;
            } else {
              TestRunner.addResult('FAILED: open was called with wrong extension. fileName: \'' + fileName + '\'');
              next();
            }
          },

          write: function(data) {
            return new Promise(resolve => {
              file._appendData(data);
              if (data.length) {
                TestRunner.addResult('PASS: write was called with data length more than zero');
                resolve();
              } else {
                TestRunner.addResult('FAILED: write was called with zero data length');
                next();
              }
            });
          },

          close: function() {
            TestRunner.addResult('PASS: close was called');
            next();
          }
        };

        profile.saveToFile();
      }
      CPUProfilerTestRunner.showProfileWhenAdded('manual');
      CPUProfilerTestRunner.waitUntilProfileViewIsShown('manual', view => saveProfileToFile(view._profileHeader));
      TestRunner.evaluateInPage('pageFunction()', function done() {});
    },

    function testLoad(next) {
      var loadedProfileData;
      function checkLoadedContent(profileView) {
        if (loadedProfileData === file._data())
          TestRunner.addResult('PASS: the content of loaded profile matches with the original profile.');
        else {
          TestRunner.addResult('FAIL: the content of loaded profile doesn\'t match with the original profile.');
          TestRunner.addResult('old: ' + file._data());
          TestRunner.addResult('new: ' + loadedProfileData);
        }
        next();
      }
      var profilesPanel = UI.panels.js_profiler;
      var profileName = file.name.substr(0, file.name.length - '.cpuprofile'.length);
      CPUProfilerTestRunner.waitUntilProfileViewIsShown(profileName, checkLoadedContent);
      profilesPanel._loadFromFile(file);
      TestRunner.addSniffer(Profiler.CPUProfileHeader.prototype, 'updateStatus', function(statusText) {
        if (!statusText.startsWith('Parsing'))
          return;
        loadedProfileData = this._jsonifiedProfile;
        setTimeout(() => UI.panels.js_profiler.showProfile(this), 0);
      }, true);
    }

  ]);
})();
