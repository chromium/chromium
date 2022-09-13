// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_FUCHSIA_H_
#define FUCHSIA_WEB_RUNNERS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_FUCHSIA_H_

#include "components/cast/named_message_port_connector/named_message_port_connector.h"

namespace fuchsia {
namespace web {
class Frame;
}
}  // namespace fuchsia

// Publishes NamedMessagePortConnector services to documents loaded in |frame|.
// OnFrameDisconnect() should be called if the FramePtr is torn down before
// |this|.
class NamedMessagePortConnectorFuchsia
    : public cast_api_bindings::NamedMessagePortConnector {
 public:
  explicit NamedMessagePortConnectorFuchsia(fuchsia::web::Frame* frame);
  ~NamedMessagePortConnectorFuchsia() override;

  NamedMessagePortConnectorFuchsia(const NamedMessagePortConnectorFuchsia&) =
      delete;
  void operator=(const NamedMessagePortConnectorFuchsia&) = delete;

  // Called when the peer Frame connection has terminated.
  void DetachFromFrame();

 private:
  fuchsia::web::Frame* frame_ = nullptr;
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_NAMED_MESSAGE_PORT_CONNECTOR_FUCHSIA_H_
