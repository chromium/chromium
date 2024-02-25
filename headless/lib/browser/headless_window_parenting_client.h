// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_PARENTING_CLIENT_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_PARENTING_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/client/window_parenting_client.h"

namespace headless {

class HeadlessWindowParentingClient
    : public aura::client::WindowParentingClient {
 public:
  explicit HeadlessWindowParentingClient(aura::Window* root_window);

  HeadlessWindowParentingClient(const HeadlessWindowParentingClient&) = delete;
  HeadlessWindowParentingClient& operator=(
      const HeadlessWindowParentingClient&) = delete;

  ~HeadlessWindowParentingClient() override;

  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds,
                                 const int64_t display) override;

 private:
  raw_ptr<aura::Window> root_window_;  // Not owned.
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_WINDOW_PARENTING_CLIENT_H_
