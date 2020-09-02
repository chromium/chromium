// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_DISPATCHER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_DISPATCHER_H_

#include "components/guest_view/renderer/guest_view_container_dispatcher.h"

namespace extensions {

class ExtensionsGuestViewContainerDispatcher
    : public guest_view::GuestViewContainerDispatcher {
 public:
  ExtensionsGuestViewContainerDispatcher();
  ExtensionsGuestViewContainerDispatcher(
      const ExtensionsGuestViewContainerDispatcher&) = delete;
  ExtensionsGuestViewContainerDispatcher& operator=(
      const ExtensionsGuestViewContainerDispatcher&) = delete;
  ~ExtensionsGuestViewContainerDispatcher() override;

 private:
  // guest_view::GuestViewContainerDispatcher implementation.
  bool HandlesMessage(const IPC::Message& message) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_DISPATCHER_H_
