// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var filePassingModule = null;
var testFilesystem;
var gotReady = false;

function handleMessage(message) {
  if (message.data == 'ready') {
    chrome.test.assertFalse(gotReady);
    gotReady = true;
    chrome.mediaGalleries.getMediaFileSystems(
        function(filesystems) {
          if (filesystems.length != 1) {
            chrome.test.fail('Wrong number of media galleries: ' +
                             filesystems.length);
            return;
          }
          testFilesystem = filesystems[0];
          var message = {
            'filesystem': testFilesystem,
            'fullPath': '/test.jpg',
            'testType': 'read_test'
          };
          filePassingModule.postMessage(message);
        });
  } else if (message.data == 'read_success'){
    var message = {
      'filesystem': testFilesystem,
      'fullPath': '/test.jpg',
      'testType': 'write_test'
    };
    filePassingModule.postMessage(message);
  } else if (message.data == 'write_success'){
    chrome.test.succeed();
  } else {
    chrome.test.fail(message.data);
  }
}

window.onload = function() {
  filePassingModule = document.getElementById('nacl_module');
  filePassingModule.addEventListener('message', handleMessage, false);
};

