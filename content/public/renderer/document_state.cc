// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/document_state.h"

namespace content {

DocumentState::DocumentState()
    : was_fetched_via_spdy_(false),
      was_alpn_negotiated_(false),
      was_alternate_protocol_available_(false),
      connection_info_(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN),
      was_load_data_with_base_url_request_(false),
      can_load_local_resources_(false) {}

DocumentState::~DocumentState() {}

std::unique_ptr<DocumentState> DocumentState::Clone() {
  std::unique_ptr<DocumentState> new_document_state(new DocumentState());
  new_document_state->set_was_fetched_via_spdy(was_fetched_via_spdy_);
  new_document_state->set_was_alpn_negotiated(was_alpn_negotiated_);
  new_document_state->set_alpn_negotiated_protocol(alpn_negotiated_protocol_);
  new_document_state->set_was_alternate_protocol_available(
      was_alternate_protocol_available_);
  new_document_state->set_connection_info(connection_info_);
  new_document_state->set_was_load_data_with_base_url_request(
      was_load_data_with_base_url_request_);
  new_document_state->set_data_url(data_url_);
  new_document_state->set_can_load_local_resources(can_load_local_resources_);
  return new_document_state;
}

}  // namespace content
