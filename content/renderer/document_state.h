// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOCUMENT_STATE_H_
#define CONTENT_RENDERER_DOCUMENT_STATE_H_

#include <memory>

#include "content/common/content_export.h"
#include "net/http/http_response_info.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "url/gurl.h"

namespace content {

class NavigationState;

// RenderFrameImpl stores an instance of this class in the "extra data" of each
// WebDocumentLoader.
class CONTENT_EXPORT DocumentState
    : public blink::WebDocumentLoader::ExtraData {
 public:
  DocumentState();
  ~DocumentState() override;

  static DocumentState* FromDocumentLoader(
      blink::WebDocumentLoader* document_loader) {
    return static_cast<DocumentState*>(document_loader->GetExtraData());
  }

  // Clones this. Passed to the document loader of a newly committed empty
  // document that replaces an existing document. This is done for discard
  // operations or javascript URL navigations such that the new document loader
  // behaves simiarly to the previous one. `navigation_state_` is not cloned
  // as this is specific to the original document.
  std::unique_ptr<blink::WebDocumentLoader::ExtraData> Clone() override;

  // For LoadDataWithBaseURL navigations, |was_load_data_with_base_url_request_|
  // is set to true and |data_url_| is set to the data URL of the navigation.
  // Otherwise, |was_load_data_with_base_url_request_| is false and |data_url_|
  // is empty.
  // NOTE: This does not actually cover all cases of LoadDataWithBaseURL
  // navigations, see comments in render_frame_impl.cc's
  // ShouldLoadDataWithBaseURL() and BuildDocumentStateFromParams() for more
  // details. Prefer calling ShouldLoadDataWithBaseURL() instead of this method.
  // TODO(https://crbug.com/1223403, https://crbug.com/1223408): Make this
  // consistent with the other LoadDataWithBaseURL checks.
  void set_was_load_data_with_base_url_request(bool value) {
    was_load_data_with_base_url_request_ = value;
  }
  bool was_load_data_with_base_url_request() const {
    return was_load_data_with_base_url_request_;
  }
  const GURL& data_url() const { return data_url_; }
  void set_data_url(const GURL& data_url) { data_url_ = data_url; }

  // True if the user agent was overridden for this page.
  bool is_overriding_user_agent() const { return is_overriding_user_agent_; }
  void set_is_overriding_user_agent(bool state) {
    is_overriding_user_agent_ = state;
  }

  // This is a fake navigation request id, which we send to the browser process
  // together with metrics. Note that renderer does not actually issue a request
  // for navigation (browser does it instead), but still reports metrics for it.
  // See content::mojom::ResourceLoadInfo.
  int request_id() const { return request_id_; }
  void set_request_id(int request_id) { request_id_ = request_id; }

  NavigationState* navigation_state() { return navigation_state_.get(); }
  void set_navigation_state(std::unique_ptr<NavigationState> navigation_state);
  void clear_navigation_state() { navigation_state_.reset(); }

 private:
  bool was_load_data_with_base_url_request_ = false;
  GURL data_url_;
  bool is_overriding_user_agent_ = false;
  int request_id_ = -1;
  std::unique_ptr<NavigationState> navigation_state_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOCUMENT_STATE_H_
