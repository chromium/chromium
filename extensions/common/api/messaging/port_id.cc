// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/messaging/port_id.h"

namespace extensions {

PortId::PortId() = default;
PortId::PortId(const base::UnguessableToken& context_id,
               int port_number,
               bool is_opener,
               mojom::SerializationFormat format)
    : context_id(context_id),
      port_number(port_number),
      is_opener(is_opener),
      serialization_format(format) {}
PortId::~PortId() = default;
PortId::PortId(PortId&& other) = default;
PortId::PortId(const PortId& other) = default;
PortId& PortId::operator=(const PortId& other) = default;

}  // namespace extensions
