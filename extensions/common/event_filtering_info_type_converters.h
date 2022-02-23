// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EVENT_FILTERING_INFO_TYPE_CONVERTERS_H_
#define EXTENSIONS_COMMON_EVENT_FILTERING_INFO_TYPE_CONVERTERS_H_

#include "extensions/common/event_filtering_info.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {
// TODO(crbug.com/1222550): Remove these converters once
// extensions::EventFilteringInfo is removed.
template <>
struct TypeConverter<extensions::mojom::EventFilteringInfoPtr,
                     extensions::EventFilteringInfo> {
  static extensions::mojom::EventFilteringInfoPtr Convert(
      const extensions::EventFilteringInfo& input);
};

template <>
struct TypeConverter<extensions::EventFilteringInfo,
                     extensions::mojom::EventFilteringInfo> {
  static extensions::EventFilteringInfo Convert(
      const extensions::mojom::EventFilteringInfo& input);
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_EVENT_FILTERING_INFO_TYPE_CONVERTERS_H_