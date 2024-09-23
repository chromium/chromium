// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"

#include "content/public/browser/render_frame_host.h"

namespace content {

DOCUMENT_USER_DATA_KEY_IMPL(RuntimeFeatureStateDocumentData);

RuntimeFeatureStateDocumentData::~RuntimeFeatureStateDocumentData() = default;

RuntimeFeatureStateDocumentData::RuntimeFeatureStateDocumentData(
    RenderFrameHost* rfh,
    const blink::RuntimeFeatureStateReadContext& read_context)
    : DocumentUserData(rfh), runtime_feature_state_read_context_(read_context) {
  runtime_feature_state_read_context_.set_origin_on_navigation(
      origin(), base::PassKey<RuntimeFeatureStateDocumentData>());
}

RuntimeFeatureStateDocumentData::RuntimeFeatureStateDocumentData(
    RenderFrameHost* rfh)
    : DocumentUserData(rfh),
      runtime_feature_state_read_context_(
          blink::RuntimeFeatureStateReadContext()) {
  runtime_feature_state_read_context_.set_origin_on_navigation(
      origin(), base::PassKey<RuntimeFeatureStateDocumentData>());
}

}  // namespace content
