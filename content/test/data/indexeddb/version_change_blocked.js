// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test()
{
  if (document.location.hash === '#tab1') {
    prepareDatabase(function () { doSetVersion(2); });
  } else if (document.location.hash === '#tab2') {
    doSetVersion(3);
  } else {
    result('fail - unexpected hash');
  }
}

function prepareDatabase(callback)
{
  // Prepare the database, then exit normally
  var delreq = window.indexedDB.deleteDatabase('version-change-blocked');
  delreq.onerror = unexpectedErrorCallback;
  delreq.onsuccess = function() {
    reOpen(callback);
  };
}

function reOpen(callback)
{
  request = indexedDB.open('version-change-blocked');
  request.onerror = unexpectedErrorCallback;
  request.onblocked = unexpectedBlockedCallback;
  request.onupgradeneeded = function() {
    db = event.target.result;
    db.createObjectStore("someobjectstore");
  };
  request.onsuccess = function() {
    db.close();
    callback();
  };
}

function doSetVersion(version)
{
  // Open the database and try a setVersion
  var openreq = window.indexedDB.open('version-change-blocked', version);
  openreq.onerror = unexpectedErrorCallback;
  var upgradeneededComplete = false;
  openreq.onblocked = function(e) {
    result('setVersion(' + version + ') blocked');
  };
  openreq.onupgradeneeded = function(e) {
    openreq.transaction.oncomplete = function(e2) {
      db = openreq.result;
      result('setVersion(' + version + ') complete');
    };
  };
}
