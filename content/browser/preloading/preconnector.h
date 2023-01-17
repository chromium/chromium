// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRECONNECTOR_H_
#define CONTENT_BROWSER_PRELOADING_PRECONNECTOR_H_

#include "content/common/content_export.h"
#include "content/public/browser/anchor_element_preconnect_delegate.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHost;

// Handles heuristics based preconnects.
// TODO(isaboori): Implement the preconnect logic directly in content/browser
// and remove the delegate.
class CONTENT_EXPORT Preconnector {
 public:
  Preconnector() = delete;
  explicit Preconnector(RenderFrameHost& render_frame_host);
  ~Preconnector();

  void MaybePreconnect(const GURL& url);

 private:
  std::unique_ptr<AnchorElementPreconnectDelegate> preconnect_delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRECONNECTOR_H_
