// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MOJOM_EVENT_DISPATCHER_MOJOM_TRAITS_H_
#define EXTENSIONS_COMMON_MOJOM_EVENT_DISPATCHER_MOJOM_TRAITS_H_

#include "extensions/common/event_filtering_info.h"
#include "extensions/common/mojom/event_dispatcher.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<extensions::mojom::EventFilteringInfoDataView,
                    extensions::EventFilteringInfo> {
  static const std::optional<GURL>& url(
      const extensions::EventFilteringInfo& filtering_info) {
    return filtering_info.url;
  }
  static const std::optional<std::string>& service_type(
      const extensions::EventFilteringInfo& filtering_info) {
    return filtering_info.service_type;
  }
  static bool has_instance_id(
      const extensions::EventFilteringInfo& filtering_info) {
    return filtering_info.instance_id.has_value();
  }
  static int instance_id(const extensions::EventFilteringInfo& filtering_info) {
    return filtering_info.instance_id.value_or(0);
  }
  static const std::optional<std::string>& window_type(
      const extensions::EventFilteringInfo& filtering_info) {
    return filtering_info.window_type;
  }
  static bool has_window_exposed_by_default(
      const extensions::EventFilteringInfo& filtering_info) {
    return filtering_info.window_exposed_by_default.has_value();
  }
  static int window_exposed_by_default(
      const extensions::EventFilteringInfo& filtering_info) {
    return filtering_info.window_exposed_by_default.value_or(0);
  }

  static bool Read(extensions::mojom::EventFilteringInfoDataView data,
                   extensions::EventFilteringInfo* out);
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_MOJOM_EVENT_DISPATCHER_MOJOM_TRAITS_H_
