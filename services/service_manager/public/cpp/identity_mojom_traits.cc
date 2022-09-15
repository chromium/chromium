// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/identity_mojom_traits.h"

#include <string>

#include "mojo/public/cpp/base/token_mojom_traits.h"

namespace mojo {

bool StructTraits<service_manager::mojom::IdentityDataView,
                  service_manager::Identity>::
    Read(service_manager::mojom::IdentityDataView data,
         service_manager::Identity* out) {
  std::string name;
  if (!data.ReadName(&name) || name.empty())
    return false;

  base::Token instance_group;
  if (!data.ReadInstanceGroup(&instance_group) || instance_group.is_zero())
    return false;

  base::Token instance_id;
  if (!data.ReadInstanceId(&instance_id))
    return false;

  base::Token globally_unique_id;
  if (!data.ReadGloballyUniqueId(&globally_unique_id) ||
      globally_unique_id.is_zero()) {
    return false;
  }

  *out = service_manager::Identity(name, instance_group, instance_id,
                                   globally_unique_id);
  return true;
}

}  // namespace mojo
