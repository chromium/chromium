// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onfetch = function(event) {
  var url = new URL(event.request.url);
  var target = url.searchParams.get('url');
  if (target) {
    event.respondWith(fetch(target, {mode: 'cors'}));
    return;
  }
  event.respondWith(fetch(event.request));
};
