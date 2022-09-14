// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for crbug.com/788464.
window.onload = function() {
  if (location.search == '') {
    window.history.replaceState({}, 'onreload', '?action=onreload');
    location.reload();
  } else if (location.search == '?action=onreload') {
    location.replace('pony.html');
  }
};
