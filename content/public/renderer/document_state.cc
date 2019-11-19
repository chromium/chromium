// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/document_state.h"

namespace content {

DocumentState::DocumentState() : was_load_data_with_base_url_request_(false) {}

DocumentState::~DocumentState() {}

std::unique_ptr<DocumentState> DocumentState::Clone() {
  std::unique_ptr<DocumentState> new_document_state(new DocumentState());
  new_document_state->set_was_load_data_with_base_url_request(
      was_load_data_with_base_url_request_);
  new_document_state->set_data_url(data_url_);
  return new_document_state;
}

}  // namespace content
