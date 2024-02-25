// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_window_parenting_client.h"

#include "ui/aura/window.h"

namespace headless {

HeadlessWindowParentingClient::HeadlessWindowParentingClient(
    aura::Window* root_window)
    : root_window_(root_window) {
  aura::client::SetWindowParentingClient(root_window_, this);
}

HeadlessWindowParentingClient::~HeadlessWindowParentingClient() {
  aura::client::SetWindowParentingClient(root_window_, nullptr);
}

aura::Window* HeadlessWindowParentingClient::GetDefaultParent(
    aura::Window* window,
    const gfx::Rect& bounds,
    const int64_t display_id) {
  return root_window_;
}

}  // namespace headless
