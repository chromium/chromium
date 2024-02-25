// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_PORT_ID_H_
#define EXTENSIONS_COMMON_API_MESSAGING_PORT_ID_H_

#include <utility>

#include "base/unguessable_token.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"

namespace extensions {

// A unique identifier for the Channel a port is on; i.e., the two-way
// communication between opener and receiver(s). We can use the pair of
// <context_id, port_number> which is the same between opener and receivers
// but unique amongst all other connections.
using ChannelId = std::pair<base::UnguessableToken, int>;

// A unique identifier for an extension port. The id is composed of three parts:
// - context_id: An UnguessableToken that uniquely identifies the creation
//               context of the port.
// - port_number: A simple identifer that uniquely identifies the port *within
//                the creation context*. That is, each creation context may
//                have a port with number '1', but there should only be a single
//                port with the number '1' in each.
// - is_opener: Whether or not this port id is for the opener port (false
//              indicating it is the receiver port).
// Additionally, this also holds `serialization_format` which is the
// preferred mojom::SerializationFormat to be used for messages sent by this
// port. A few more notes:
// - There should only be a single existent opener port. That is, in all the
//   contexts, there should only be one with a given context_id, port_number,
//   and is_opener set to true. However, each port can have multiple receivers,
//   thus there may be multiple ports with a given context_id, port_number, and
//   is_opener set to false.
// - The context_id and port_number refer to the values at *creation*, and are
//   conceptually immutable. Receiver ports will always have a context id other
//   than the one they are hosted in (since we don't dispatch messages to the
//   same context). Only in the case of opener ports do these identify the
//   current context.
// - Context id and port number are set in the renderer process. Theoretically,
//   this means that multiple contexts could have the same id. However, GUIDs
//   are sufficiently random as to be globally unique in practice (the chance
//   of a duplicate is about the same as the sun exploding right now).
// - The `serialization_format` for a port may sometimes differ from that of the
//   Messages sent on the port. For example this can happen if a message can't
//   be structure cloned in which case we fallback to JSON.
struct PortId {
  // See class comments for the description of these fields.
  base::UnguessableToken context_id;
  int port_number = 0;
  bool is_opener = false;
  mojom::SerializationFormat serialization_format =
      mojom::SerializationFormat::kJson;

  PortId();
  PortId(const base::UnguessableToken& context_id,
         int port_number,
         bool is_opener,
         mojom::SerializationFormat format);
  ~PortId();
  PortId(PortId&& other);
  PortId(const PortId& other);
  PortId& operator=(const PortId& other);

  ChannelId GetChannelId() const {
    return std::make_pair(context_id, port_number);
  }

  // Returns the identifier for the opposite port(s). That is, a call on a
  // receiver port returns the ID of the opener, and a call on the opener port
  // returns the ID of all receivers.
  PortId GetOppositePortId() const {
    return PortId(context_id, port_number, !is_opener, serialization_format);
  }

  bool operator==(const PortId& other) const;
  bool operator<(const PortId& other) const;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_PORT_ID_H_
