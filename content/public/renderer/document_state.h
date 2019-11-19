// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_DOCUMENT_STATE_H_
#define CONTENT_PUBLIC_RENDERER_DOCUMENT_STATE_H_

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "net/http/http_response_info.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "url/gurl.h"

namespace content {

// The RenderView stores an instance of this class in the "extra data" of each
// WebDocumentLoader (see RenderView::DidCreateDataSource).
class CONTENT_EXPORT DocumentState : public blink::WebDocumentLoader::ExtraData,
                                     public base::SupportsUserData {
 public:
  DocumentState();
  ~DocumentState() override;

  static DocumentState* FromDocumentLoader(
      blink::WebDocumentLoader* document_loader) {
    return static_cast<DocumentState*>(document_loader->GetExtraData());
  }

  // Returns a copy of the DocumentState. This is a shallow copy,
  // user data is not copied.
  std::unique_ptr<DocumentState> Clone();

  // For LoadDataWithBaseURL navigations, |was_load_data_with_base_url_request_|
  // is set to true and |data_url_| is set to the data URL of the navigation.
  // Otherwise, |was_load_data_with_base_url_request_| is false and |data_url_|
  // is empty.
  void set_was_load_data_with_base_url_request(bool value) {
    was_load_data_with_base_url_request_ = value;
  }
  bool was_load_data_with_base_url_request() const {
    return was_load_data_with_base_url_request_;
  }
  const GURL& data_url() const {
    return data_url_;
  }
  void set_data_url(const GURL& data_url) {
    data_url_ = data_url;
  }

 private:
  bool was_load_data_with_base_url_request_;
  GURL data_url_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_DOCUMENT_STATE_H_
