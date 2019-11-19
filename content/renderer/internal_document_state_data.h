// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_
#define CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "net/nqe/effective_connection_type.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "url/gurl.h"

namespace blink {
class WebDocumentLoader;
}

namespace content {

class DocumentState;
class NavigationState;

// Stores internal state per WebDocumentLoader.
class InternalDocumentStateData : public base::SupportsUserData::Data {
 public:
  InternalDocumentStateData();
  ~InternalDocumentStateData() override;

  static InternalDocumentStateData* FromDocumentLoader(
      blink::WebDocumentLoader* document_loader);
  static InternalDocumentStateData* FromDocumentState(DocumentState* ds);

  void CopyFrom(InternalDocumentStateData* other);

  // True if the user agent was overridden for this page.
  bool is_overriding_user_agent() const { return is_overriding_user_agent_; }
  void set_is_overriding_user_agent(bool state) {
    is_overriding_user_agent_ = state;
  }

  // True if we have to reset the scroll and scale state of the page
  // after the provisional load has been committed.
  bool must_reset_scroll_and_scale_state() const {
    return must_reset_scroll_and_scale_state_;
  }
  void set_must_reset_scroll_and_scale_state(bool state) {
    must_reset_scroll_and_scale_state_ = state;
  }

  net::EffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }
  void set_effective_connection_type(
      net::EffectiveConnectionType effective_connection_type) {
    effective_connection_type_ = effective_connection_type;
  }

  // This is a fake navigation request id, which we send to the browser process
  // together with metrics. Note that renderer does not actually issue a request
  // for navigation (browser does it instead), but still reports metrics for it.
  // See content::mojom::ResourceLoadInfo.
  int request_id() const { return request_id_; }
  void set_request_id(int request_id) { request_id_ = request_id; }

  NavigationState* navigation_state() { return navigation_state_.get(); }
  void set_navigation_state(std::unique_ptr<NavigationState> navigation_state);

 private:
  bool is_overriding_user_agent_;
  bool must_reset_scroll_and_scale_state_;
  net::EffectiveConnectionType effective_connection_type_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  int request_id_ = -1;
  std::unique_ptr<NavigationState> navigation_state_;

  DISALLOW_COPY_AND_ASSIGN(InternalDocumentStateData);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_
