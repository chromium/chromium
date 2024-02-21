// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener("connect", (evt) => {
    evt.ports[0].postMessage("loaded");
});
