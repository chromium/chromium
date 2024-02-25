// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_filter_mojom_traits.h"
#include "mojo/public/cpp/base/token_mojom_traits.h"

namespace mojo {

bool StructTraits<service_manager::mojom::ServiceFilterDataView,
                  service_manager::ServiceFilter>::
    Read(service_manager::mojom::ServiceFilterDataView data,
         service_manager::ServiceFilter* out) {
  std::string service_name;
  if (!data.ReadServiceName(&service_name))
    return false;
  std::optional<base::Token> instance_group;
  if (!data.ReadInstanceGroup(&instance_group))
    return false;
  std::optional<base::Token> instance_id;
  if (!data.ReadInstanceId(&instance_id))
    return false;
  std::optional<base::Token> globally_unique_id;
  if (!data.ReadGloballyUniqueId(&globally_unique_id))
    return false;
  out->set_service_name(service_name);
  out->set_instance_group(instance_group);
  out->set_instance_id(instance_id);
  out->set_globally_unique_id(globally_unique_id);
  return true;
}

}  // namespace mojo
