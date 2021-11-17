// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_filtering_info_type_converters.h"

namespace mojo {

// static
extensions::mojom::EventFilteringInfoPtr
TypeConverter<extensions::mojom::EventFilteringInfoPtr,
              extensions::EventFilteringInfo>::
    Convert(const extensions::EventFilteringInfo& input) {
  extensions::mojom::EventFilteringInfoPtr output =
      extensions::mojom::EventFilteringInfo::New();
  output->url = input.url;
  output->service_type = input.service_type;
  output->has_instance_id = input.instance_id.has_value();
  if (output->has_instance_id)
    output->instance_id = input.instance_id.value();
  output->window_type = input.window_type;
  output->has_window_exposed_by_default =
      input.window_exposed_by_default.has_value();
  if (output->has_window_exposed_by_default)
    output->window_exposed_by_default = input.window_exposed_by_default.value();
  return output;
}

// static
extensions::EventFilteringInfo
TypeConverter<extensions::EventFilteringInfo,
              extensions::mojom::EventFilteringInfo>::
    Convert(const extensions::mojom::EventFilteringInfo& input) {
  extensions::EventFilteringInfo output;
  output.url = input.url;
  output.service_type = input.service_type;
  if (input.has_instance_id)
    output.instance_id = input.instance_id;
  output.window_type = input.window_type;
  if (input.has_window_exposed_by_default)
    output.window_exposed_by_default = input.window_exposed_by_default;
  return output;
}

}  // namespace mojo