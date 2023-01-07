// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('install', function(event) {
  event.waitUntil(self.skipWaiting());
});

self.addEventListener('activate', function(event) {
  event.waitUntil(self.clients.claim());
});

