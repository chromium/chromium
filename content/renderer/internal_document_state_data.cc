// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/internal_document_state_data.h"

#include "base/memory/ptr_util.h"
#include "content/public/renderer/document_state.h"
#include "content/renderer/navigation_state.h"
#include "third_party/blink/public/web/web_document_loader.h"

namespace content {

namespace {

// Key InternalDocumentStateData is stored under in DocumentState.
const char kUserDataKey[] = "InternalDocumentStateData";

}

InternalDocumentStateData::InternalDocumentStateData()
    : http_status_code_(0),
      is_overriding_user_agent_(false),
      must_reset_scroll_and_scale_state_(false),
      cache_policy_override_set_(false),
      cache_policy_override_(blink::mojom::FetchCacheMode::kDefault) {}

// static
InternalDocumentStateData* InternalDocumentStateData::FromDocumentLoader(
    blink::WebDocumentLoader* document_loader) {
  return FromDocumentState(
      static_cast<DocumentState*>(document_loader->GetExtraData()));
}

// static
InternalDocumentStateData* InternalDocumentStateData::FromDocumentState(
    DocumentState* ds) {
  if (!ds)
    return nullptr;
  InternalDocumentStateData* data = static_cast<InternalDocumentStateData*>(
      ds->GetUserData(&kUserDataKey));
  if (!data) {
    data = new InternalDocumentStateData;
    ds->SetUserData(&kUserDataKey, base::WrapUnique(data));
  }
  return data;
}

InternalDocumentStateData::~InternalDocumentStateData() {
}

void InternalDocumentStateData::CopyFrom(InternalDocumentStateData* other) {
  http_status_code_ = other->http_status_code_;
  is_overriding_user_agent_ = other->is_overriding_user_agent_;
  must_reset_scroll_and_scale_state_ =
      other->must_reset_scroll_and_scale_state_;
  cache_policy_override_set_ = other->cache_policy_override_set_;
  cache_policy_override_ = other->cache_policy_override_;
}

void InternalDocumentStateData::set_navigation_state(
    std::unique_ptr<NavigationState> navigation_state) {
  navigation_state_ = std::move(navigation_state);
}

}  // namespace content
