// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_VIEWER_INTERFACE_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_VIEWER_INTERFACE_H_

#import "components/dom_distiller/core/dom_distiller_request_view_base.h"

class GURL;

namespace dom_distiller {
class DistilledPagePrefs;
}  // namespace dom_distiller

// An interface for a dom_distiller ViewRequestDelegate that distills a URL and
// calls the given callback with the distilled HTML string and the images it
// contains.
class DistillerViewerInterface
    : public dom_distiller::DomDistillerRequestViewBase {
 public:
  struct ImageInfo {
    // The url of the image.
    GURL url;
    // The image data as a string.
    std::string data;
  };
  using DistillationFinishedCallback =
      base::OnceCallback<void(const GURL& url,
                              const std::string& html,
                              const std::vector<ImageInfo>& images,
                              const std::string& title,
                              const std::string& csp_nonce)>;

  DistillerViewerInterface(dom_distiller::DistilledPagePrefs* prefs)
      : dom_distiller::DomDistillerRequestViewBase(prefs) {}

  DistillerViewerInterface(const DistillerViewerInterface&) = delete;
  DistillerViewerInterface& operator=(const DistillerViewerInterface&) = delete;

  ~DistillerViewerInterface() override {}

  // dom_distiller::DomDistillerRequestViewBase implementation.
  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override = 0;
  void SendJavaScript(const std::string& buffer) override = 0;
};

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_VIEWER_INTERFACE_H_
