// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onEmbedRequested.addListener(function(request) {
  request.allow('main.html');
});
