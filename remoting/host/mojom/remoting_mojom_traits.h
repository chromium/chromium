// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_
#define REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "remoting/host/mojom/clipboard.mojom-shared.h"
#include "remoting/proto/event.pb.h"

namespace mojo {

template <>
class mojo::StructTraits<remoting::mojom::ClipboardEventDataView,
                         ::remoting::protocol::ClipboardEvent> {
 public:
  static const std::string& mime_type(
      const ::remoting::protocol::ClipboardEvent& event) {
    return event.mime_type();
  }

  static const std::string& data(
      const ::remoting::protocol::ClipboardEvent& event) {
    return event.data();
  }

  static bool Read(remoting::mojom::ClipboardEventDataView data_view,
                   ::remoting::protocol::ClipboardEvent* out_event);
};

}  // namespace mojo

#endif  // REMOTING_HOST_MOJOM_REMOTING_MOJOM_TRAITS_H_
