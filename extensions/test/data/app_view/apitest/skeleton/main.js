// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onEmbedRequested.addListener(function(request) {
  if (!request.data.foo) {
    request.allow('main.html');
    return;
  } else if (request.data.foo == 'bar') {
    request.deny();
  } else if (request.data.foo == 'bleep') {
    request.allow('main.html');
  }
});

