// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keeps this service worker in the `activating` state until the response for
// `/service_worker/hold_activation` is received.
self.addEventListener('activate', e => {
  e.waitUntil(fetch('/service_worker/hold_activation'));
});
