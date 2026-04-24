// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/host_types.h"

#include <iostream>

#include "base/no_destructor.h"

namespace remoting {
namespace {

class SingleProcessHostType : public HostType {
 public:
  ~SingleProcessHostType() override;
  std::string_view description() const override;
  bool is_multi_process() const override;
};

class GdmManagedHostType : public HostType {
 public:
  ~GdmManagedHostType() override;
  std::string_view description() const override;
  bool is_multi_process() const override;
};

SingleProcessHostType::~SingleProcessHostType() = default;

std::string_view SingleProcessHostType::description() const {
  return "Single-process host (legacy). This host type is run as the user, "
         "and supports X11.";
}

bool SingleProcessHostType::is_multi_process() const {
  return false;
}

GdmManagedHostType::~GdmManagedHostType() = default;

std::string_view GdmManagedHostType::description() const {
  return "GDM-managed multi-process host. This host type only supports GNOME "
         "49+ in Wayland mode. You will be presented with the login screen "
         "the first time you connect. Note that it doesn't support running "
         "multiple hosts for different users.";
}

bool GdmManagedHostType::is_multi_process() const {
  return true;
}

}  // namespace

// static
const base::flat_map<std::string_view, const HostType*>&
HostType::GetHostTypes() {
  static const base::NoDestructor<SingleProcessHostType> kSingleProcess;
  static const base::NoDestructor<GdmManagedHostType> kGdmManaged;
  static const base::NoDestructor<
      base::flat_map<std::string_view, const HostType*>>
      kHostTypes({
          {"single-process", kSingleProcess.get()},
          {"gdm-managed", kGdmManaged.get()},
      });
  return *kHostTypes;
}

// static
const HostType* HostType::Find(std::string_view name) {
  const auto& host_types = GetHostTypes();
  auto it = host_types.find(name);
  return it == host_types.end() ? nullptr : it->second;
}

// static
void HostType::PrintHostTypeHelp() {
  std::cerr << "Supported host types:\n";
  for (const auto& [name, type] : GetHostTypes()) {
    std::cerr << "  " << name << "\n    " << type->description() << "\n\n";
  }
}

}  // namespace remoting
