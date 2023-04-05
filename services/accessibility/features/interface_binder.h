// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_INTERFACE_BINDER_H_
#define SERVICES_ACCESSIBILITY_FEATURES_INTERFACE_BINDER_H_

#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

namespace ax {

// Virtual class that can bind to a GenericPendingReceiver to
// implement one end of a mojo pipe. The V8Manager maps interface names
// to InterfaceBinders when Mojo in JS receives a GenericPendingReceiver.
class InterfaceBinder {
 public:
  virtual ~InterfaceBinder() = default;

  // Returns true if `interface_name` is the interface for this InterfaceBinder.
  virtual bool MatchesInterface(const std::string& interface_name) = 0;

  // Binds a GenericPendingReceiver to an implementation for this
  // interface.
  virtual void BindReceiver(mojo::GenericPendingReceiver receiver) = 0;
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_INTERFACE_BINDER_H_
