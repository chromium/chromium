// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gdm_managed_host_type.h"

#include <string_view>

#include "base/no_destructor.h"
#include "remoting/host/linux/host_types.h"

namespace remoting {

namespace {

// Multi-process host whose sessions are managed by GDM, which is the case if
// there is no session routing config file.
class GdmManagedHostType : public MultiProcessHostType {
 public:
  std::string_view name() const override { return "gdm-managed"; }
  std::string_view description() const override {
    return "GDM-managed multi-process host. This host type only supports GNOME "
           "49+ in Wayland mode. You will be presented with the login screen "
           "the first time you connect. Note that it doesn't support running "
           "multiple hosts for different users.";
  }
  bool requires_local_user() const override { return false; }

 protected:
  bool SetUpState(const HostTypeContext& context) const override {
    if (!MultiProcessHostType::SetUpState(context)) {
      return false;
    }
    // GDM only manages the sessions if the session routing config file doesn't
    // exist.
    return DeleteSessionRoutingConfig(*context.env);
  }
};

}  // namespace

const HostType& GetGdmManagedHostType() {
  static const base::NoDestructor<GdmManagedHostType> instance;
  return *instance;
}

}  // namespace remoting
