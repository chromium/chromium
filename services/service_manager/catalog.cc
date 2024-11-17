// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/catalog.h"

#include <vector>

#include "base/memory/raw_ptr.h"

namespace service_manager {

namespace {

std::map<Manifest::ServiceName, raw_ptr<const Manifest, CtnExperimental>>
CreateManifestMap(const std::vector<Manifest>& manifests) {
  std::map<Manifest::ServiceName, raw_ptr<const Manifest, CtnExperimental>> map;
  for (const auto& manifest : manifests) {
    map[manifest.service_name] = &manifest;
    for (const auto& entry : CreateManifestMap(manifest.packaged_services))
      map[entry.first] = entry.second;
  }
  return map;
}

std::map<Manifest::ServiceName, raw_ptr<const Manifest, CtnExperimental>>
CreateParentManifestMap(const std::vector<Manifest>& manifests) {
  std::map<Manifest::ServiceName, raw_ptr<const Manifest, CtnExperimental>> map;
  for (const auto& parent : manifests) {
    for (const auto& child : parent.packaged_services)
      map[child.service_name] = &parent;
    for (const auto& entry : CreateParentManifestMap(parent.packaged_services))
      map[entry.first] = entry.second;
  }
  return map;
}

}  // namespace

Catalog::Catalog(const std::vector<Manifest>& manifests)
    : manifests_(manifests),
      manifest_map_(CreateManifestMap(manifests_)),
      parent_manifest_map_(CreateParentManifestMap(manifests_)) {}

Catalog::~Catalog() = default;

const Manifest* Catalog::GetManifest(
    const Manifest::ServiceName& service_name) {
  const auto it = manifest_map_.find(service_name);
  if (it == manifest_map_.end())
    return nullptr;
  return it->second;
}

const Manifest* Catalog::GetParentManifest(
    const Manifest::ServiceName& service_name) {
  auto it = parent_manifest_map_.find(service_name);
  if (it == parent_manifest_map_.end())
    return nullptr;

  return it->second;
}

}  // namespace service_manager
