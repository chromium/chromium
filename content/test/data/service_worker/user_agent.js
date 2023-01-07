// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', (event) => {
  // Responds to requests containing "user_agent_sw" with the value of
  // navigator.userAgent.
  if (event.request.url.includes('user_agent_sw')) {
    event.respondWith(new Response(navigator.userAgent));
  }
});
