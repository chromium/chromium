// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_NAVIGATION_CONTENT_NAVIGATION_CONTEXT_H_
#define IOS_WEB_CONTENT_NAVIGATION_CONTENT_NAVIGATION_CONTEXT_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/raw_ref.h"
#import "content/public/browser/navigation_handle_user_data.h"
#import "ios/web/public/navigation/navigation_context.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace web {

// Wraps a content::NavigationHandle. The lifetime of these instances is tied
// to the corresponding NavigationHandle (via NavigationHandleUserData).
class ContentNavigationContext
    : public NavigationContext,
      public content::NavigationHandleUserData<ContentNavigationContext> {
 public:
  static NavigationContext* GetOrCreate(content::NavigationHandle* handle,
                                        WebState* web_state);
  ~ContentNavigationContext() override;

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

 private:
  friend content::NavigationHandleUserData<ContentNavigationContext>;
  ContentNavigationContext(content::NavigationHandle& handle,
                           WebState* web_state);

  raw_ref<content::NavigationHandle> handle_;
  raw_ptr<WebState> web_state_ = nullptr;

  // We lazily populate this in GetError. Since the underlying NavigationHandle
  // is unmodified, this function is still semantically const.
  mutable NSError* error_ = nil;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_NAVIGATION_CONTENT_NAVIGATION_CONTEXT_H_
