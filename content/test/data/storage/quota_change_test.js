// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onQuotaChange(e) {
  done();
}

function test() {
  if (!navigator.storage.onquotachange) {
    debug('Adding change listener.');

    navigator.storage.addEventListener('quotachange',
                                         onQuotaChange);
    debug('Change listener added, navigating to #ready.');
    document.location.hash = '#ready';
  } else {
    debug('This test requires navigator.storage.onquotachange');
  }
}
