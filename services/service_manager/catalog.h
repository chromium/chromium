// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_CATALOG_H_
#define SERVICES_SERVICE_MANAGER_CATALOG_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "services/service_manager/public/cpp/manifest.h"

namespace service_manager {

// Owns the Service Manager's collections of Manifests and provides convenient
// indexes for fast lookup by name and resolution of parent-child relationships.
class Catalog {
 public:
  // Constructs a catalog over a set of Manifests to use for lookup.
  explicit Catalog(const std::vector<Manifest>& manifests);

  Catalog(const Catalog&) = delete;
  Catalog& operator=(const Catalog&) = delete;

  ~Catalog();

  // Returns manifest data for the service named by |service_name|. If no
  // service is known by that name, this returns null.
  const Manifest* GetManifest(const Manifest::ServiceName& service_name);

  // Returns manifest data for the parent of the service named by
  // |service_name|. If the named service has no parent (i.e. it's not packaged
  // within another service) then this returns null.
  const Manifest* GetParentManifest(const Manifest::ServiceName& service_name);

 private:
  // The set of all top-level manifests known to the Service Manager.
  const std::vector<Manifest> manifests_;

  // Maintains a mapping from service name to manifest for quick lookup of any
  // manifest regardless of whether it's packaged. The values in this map refer
  // to objects owned by |manifests_| above.
  const std::map<Manifest::ServiceName,
                 raw_ptr<const Manifest, CtnExperimental>>
      manifest_map_;

  // Maintains a mapping from service name to parent manifest for quick
  // reverse-lookup of packaged service relationships. The values in this map
  // refer to objects owned by |manifests_| above.
  const std::map<Manifest::ServiceName,
                 raw_ptr<const Manifest, CtnExperimental>>
      parent_manifest_map_;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_CATALOG_H_
