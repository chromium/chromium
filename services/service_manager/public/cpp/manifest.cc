// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/manifest.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace service_manager {

Manifest::Manifest() = default;
Manifest::Manifest(const Manifest&) = default;
Manifest::Manifest(Manifest&&) = default;
Manifest::~Manifest() = default;
Manifest& Manifest::operator=(const Manifest&) = default;
Manifest& Manifest::operator=(Manifest&&) = default;

Manifest::Options::Options() = default;
Manifest::Options::Options(const Options&) = default;
Manifest::Options::Options(Options&&) = default;
Manifest::Options::~Options() = default;
Manifest::Options& Manifest::Options::operator=(Options&&) = default;
Manifest::Options& Manifest::Options::operator=(const Options&) = default;

Manifest& Manifest::Amend(Manifest other) {
  for (const auto& capability_entry : other.exposed_capabilities) {
    const auto& capability_name = capability_entry.first;
    const auto& interface_names = capability_entry.second;
    for (const auto& interface_name : interface_names)
      exposed_capabilities[capability_name].insert(interface_name);
  }

  for (const auto& filter_entry : other.exposed_interface_filter_capabilities) {
    const auto& filter_name = filter_entry.first;
    const auto& capabilities_map = filter_entry.second;
    for (const auto& capability_entry : capabilities_map) {
      const auto& capability_name = capability_entry.first;
      const auto& interface_names = capability_entry.second;
      auto& exposed_capability =
          exposed_interface_filter_capabilities[filter_name][capability_name];
      for (const auto& interface_name : interface_names)
        exposed_capability.insert(interface_name);
    }
  }

  for (const auto& capability_entry : other.required_capabilities) {
    const auto& service = capability_entry.first;
    const auto& capability_names = capability_entry.second;
    for (const auto& capability_name : capability_names)
      required_capabilities[service].insert(capability_name);
  }

  for (const auto& filter_entry :
       other.required_interface_filter_capabilities) {
    const auto& filter_name = filter_entry.first;
    const auto& capability_maps = filter_entry.second;
    for (const auto& capability_entry : capability_maps) {
      const auto& service = capability_entry.first;
      const auto& capability_names = capability_entry.second;
      auto& required =
          required_interface_filter_capabilities[filter_name][service];
      for (const auto& capability_name : capability_names)
        required.insert(capability_name);
    }
  }

  for (auto& manifest : other.packaged_services)
    packaged_services.emplace_back(std::move(manifest));
  for (auto& file_info : other.preloaded_files)
    preloaded_files.emplace_back(std::move(file_info));

  return *this;
}

}  // namespace service_manager
