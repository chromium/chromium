// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// global message var is provided by this script module.
import {message} from "./worker_imported_module.js";

self.addEventListener('fetch', event => {
  event.respondWith(new Response(message));
});
self.addEventListener('activate', event => {
  event.waitUntil(clients.claim());
});