// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

class AssociatedGroupController;

// AssociatedGroup refers to all the interface endpoints running at one end of a
// message pipe.
// It is thread safe and cheap to make copies.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) AssociatedGroup {
 public:
  AssociatedGroup();

  explicit AssociatedGroup(scoped_refptr<AssociatedGroupController> controller);

  explicit AssociatedGroup(const ScopedInterfaceEndpointHandle& handle);

  AssociatedGroup(const AssociatedGroup& other);

  ~AssociatedGroup();

  AssociatedGroup& operator=(const AssociatedGroup& other);

  // The return value of this getter if this object is initialized with a
  // ScopedInterfaceEndpointHandle:
  //   - If the handle is invalid, the return value will always be null.
  //   - If the handle is valid and non-pending, the return value will be
  //     non-null and remain unchanged even if the handle is later reset.
  //   - If the handle is pending asssociation, the return value will initially
  //     be null, change to non-null when/if the handle is associated, and
  //     remain unchanged ever since.
  AssociatedGroupController* GetController();

 private:
  base::RepeatingCallback<AssociatedGroupController*()> controller_getter_;
  scoped_refptr<AssociatedGroupController> controller_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_H_
