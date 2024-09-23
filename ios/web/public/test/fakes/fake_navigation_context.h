// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_NAVIGATION_CONTEXT_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_NAVIGATION_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#import "ios/web/public/navigation/navigation_context.h"
#include "url/gurl.h"

namespace web {

class WebState;

// Tracks information related to a single navigation.
class FakeNavigationContext : public NavigationContext {
 public:
  FakeNavigationContext(const FakeNavigationContext&) = delete;
  FakeNavigationContext& operator=(const FakeNavigationContext&) = delete;

  ~FakeNavigationContext() override;
  FakeNavigationContext();

  // NavigationContext overrides:
  WebState* GetWebState() override;
  int64_t GetNavigationId() const override;
  const GURL& GetUrl() const override;
  bool HasUserGesture() const override;
  ui::PageTransition GetPageTransition() const override;
  bool IsSameDocument() const override;
  bool HasCommitted() const override;
  bool IsDownload() const override;
  bool IsPost() const override;
  NSError* GetError() const override;
  net::HttpResponseHeaders* GetResponseHeaders() const override;
  bool IsRendererInitiated() const override;
  HttpsUpgradeType GetFailedHttpsUpgradeType() const override;

  // Setters for navigation context data members.
  void SetWebState(WebState* web_state);
  void SetUrl(const GURL& url);
  void SetHasUserGesture(bool has_user_gesture);
  void SetPageTransition(ui::PageTransition transition);
  void SetIsSameDocument(bool same_document);
  void SetHasCommitted(bool has_committed);
  void SetIsDownload(bool is_download);
  void SetIsPost(bool is_post);
  void SetError(NSError* error);
  void SetResponseHeaders(
      const scoped_refptr<net::HttpResponseHeaders>& response_headers);
  void SetIsRendererInitiated(bool renderer_initiated);

 private:
  raw_ptr<WebState> web_state_;
  int64_t navigation_id_ = 0;
  GURL url_;
  bool has_user_gesture_ = false;
  ui::PageTransition page_transition_ = ui::PAGE_TRANSITION_LINK;
  bool same_document_ = false;
  bool has_committed_ = false;
  bool is_download_ = false;
  bool is_post_ = false;
  __strong NSError* error_ = nil;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  bool renderer_initiated_ = false;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_NAVIGATION_CONTEXT_H_
