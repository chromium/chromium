// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojom/remoting_mojom_traits.h"

namespace mojo {

// static
bool mojo::StructTraits<remoting::mojom::ClipboardEventDataView,
                        ::remoting::protocol::ClipboardEvent>::
    Read(remoting::mojom::ClipboardEventDataView data_view,
         ::remoting::protocol::ClipboardEvent* out_event) {
  std::string mime_type;
  if (!data_view.ReadMimeType(&mime_type)) {
    return false;
  }
  out_event->set_mime_type(std::move(mime_type));
  std::string data;
  if (!data_view.ReadData(&data)) {
    return false;
  }
  out_event->set_data(std::move(data));
  return true;
}

}  // namespace mojo
