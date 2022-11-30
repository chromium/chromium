// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/associated_group_controller.h"

#include "mojo/public/cpp/bindings/associated_group.h"

namespace mojo {

AssociatedGroupController::~AssociatedGroupController() {}

ScopedInterfaceEndpointHandle
AssociatedGroupController::CreateScopedInterfaceEndpointHandle(InterfaceId id) {
  return ScopedInterfaceEndpointHandle(id, this);
}

bool AssociatedGroupController::NotifyAssociation(
    ScopedInterfaceEndpointHandle* handle_to_send,
    InterfaceId id) {
  return handle_to_send->NotifyAssociation(id, this);
}

}  // namespace mojo
