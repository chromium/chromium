// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test()
{
  var n = 25;
  for (var i = 0; i < n; i++) {
    indexedDB.open("bug_84933_" + i.toString()).onsuccess = function() {
      window.close();
    };
  }
}
