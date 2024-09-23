// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/document_state.h"

#include "content/renderer/navigation_state.h"

namespace content {

DocumentState::DocumentState() {}

DocumentState::~DocumentState() {}

std::unique_ptr<blink::WebDocumentLoader::ExtraData> DocumentState::Clone() {
  auto cloned_document_state = std::make_unique<DocumentState>();
  cloned_document_state->set_was_load_data_with_base_url_request(
      was_load_data_with_base_url_request_);
  cloned_document_state->set_data_url(data_url_);
  cloned_document_state->set_is_overriding_user_agent(
      is_overriding_user_agent_);
  cloned_document_state->set_request_id(request_id_);
  return cloned_document_state;
}

void DocumentState::set_navigation_state(
    std::unique_ptr<NavigationState> navigation_state) {
  navigation_state_ = std::move(navigation_state);
}

}  // namespace content
