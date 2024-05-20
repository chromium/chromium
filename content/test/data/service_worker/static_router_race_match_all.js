// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
importScripts('./race_network_request_base.js');

self.addEventListener('install', e => {
  e.addRoutes([
    {
      condition: {urlPattern: {pathname: '/service_worker/no_race'}},
      source: 'fetch-event'
    },
    {
      condition: {urlPattern: new URLPattern({})},
      source: 'race-network-and-fetch-handler'
    }
  ]);
  self.skipWaiting();
});
