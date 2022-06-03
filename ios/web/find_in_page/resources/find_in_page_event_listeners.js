// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Requires __crWeb.findInPage
goog.provide('__crWeb.findInPageEventListeners');

window.addEventListener('pagehide', __gCrWeb.findInPage.stop);
