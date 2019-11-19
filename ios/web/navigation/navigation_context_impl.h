// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_CONTEXT_IMPL_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_CONTEXT_IMPL_H_

#import <WebKit/WebKit.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/elapsed_timer.h"
#import "ios/web/public/navigation/navigation_context.h"
#include "url/gurl.h"

namespace web {

class NavigationItemImpl;

// Tracks information related to a single navigation.
class NavigationContextImpl : public NavigationContext {
 public:
  // Creates navigation context for successful navigation to a different page.
  // Response headers will be null, and it will not be marked as committed or
  // as a download.
  static std::unique_ptr<NavigationContextImpl> CreateNavigationContext(
      WebState* web_state,
      const GURL& url,
      bool has_user_gesture,
      ui::PageTransition page_transition,
      bool is_renderer_initiated);

#ifndef NDEBUG
  // Returns human readable description of this object.
  NSString* GetDescription() const;
#endif  // NDEBUG

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
  ~NavigationContextImpl() override;

  // Setters for navigation context data members.
  void SetUrl(const GURL& url);
  void SetIsSameDocument(bool is_same_document);
  void SetHasCommitted(bool has_committed);
  void SetIsDownload(bool is_download);
  void SetIsPost(bool is_post);
  void SetError(NSError* error);
  void SetResponseHeaders(
      const scoped_refptr<net::HttpResponseHeaders>& response_headers);

  // Get elapsed time since context was created.
  base::TimeDelta GetElapsedTimeSinceCreation() const;

  // Optional unique id of the navigation item associated with this navigaiton.
  int GetNavigationItemUniqueID() const;
  void SetNavigationItemUniqueID(int unique_id);

  // Optional WKNavigationType of the associated navigation in WKWebView.
  void SetWKNavigationType(WKNavigationType wk_navigation_type);
  WKNavigationType GetWKNavigationType() const;

  // true if this navigation context is a -[WKWebView loadHTMLString:baseURL:]
  // navigation used to load Error page into web view. IsLoadingErrorPage() and
  // IsLoadingHtmlString() are mutually exclusive.
  bool IsLoadingErrorPage() const;
  void SetLoadingErrorPage(bool is_loading_error_page);

  // true if this navigation context is a -[WKWebView loadHTMLString:baseURL:]
  // navigation. IsLoadingErrorPage() and IsLoadingHtmlString() are mutually
  // exclusive.
  bool IsLoadingHtmlString() const;
  void SetLoadingHtmlString(bool is_loading_html);

  // true if this navigation context is a placeholder navigation associated with
  // a native view URL and the native content is already presented.
  bool IsNativeContentPresented() const;
  void SetIsNativeContentPresented(bool is_native_content_presented);

  // true if this navigation context is a placeholder navigation.
  bool IsPlaceholderNavigation() const;
  void SetPlaceholderNavigation(bool flag);

  // MIMEType of the navigation.
  void SetMimeType(NSString* mime_type);
  NSString* GetMimeType() const;

  // Returns pending navigation item.
  NavigationItemImpl* GetItem();

  // Similar to GetItem(), but this method transfers the ownership to the
  // caller. Clients may use this method to commit navigation item or transfer
  // the item to a different navigation context or back to the navigation
  // manager.
  std::unique_ptr<NavigationItemImpl> ReleaseItem();

  // Stores pending navigation item. Clients may use this method to store
  // pending navigation item after navigation context was created.
  void SetItem(std::unique_ptr<NavigationItemImpl> item);

 private:
  NavigationContextImpl(WebState* web_state,
                        const GURL& url,
                        bool has_user_gesture,
                        ui::PageTransition page_transition,
                        bool is_renderer_initiated);

  WebState* web_state_ = nullptr;
  int64_t navigation_id_ = 0;
  GURL url_;
  bool has_user_gesture_ = false;
  const ui::PageTransition page_transition_;
  bool is_same_document_ = false;
  bool has_committed_ = false;
  bool is_download_ = false;
  bool is_post_ = false;
  NSError* error_ = nil;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  bool is_renderer_initiated_ = false;
  int navigation_item_unique_id_ = -1;
  WKNavigationType wk_navigation_type_ = WKNavigationTypeOther;
  bool is_loading_error_page_ = false;
  bool is_loading_html_string_ = false;
  bool is_native_content_presented_ = false;
  bool is_placeholder_navigation_ = false;
  NSString* mime_type_ = nil;
  base::ElapsedTimer elapsed_timer_;

  // Holds pending navigation item in this object. Pending item is stored in
  // NavigationContext after context is created. The item is still stored in
  // NavigationManager if the navigated was requested, but context does not yet
  // exist or when navigation was aborted.
  std::unique_ptr<NavigationItemImpl> item_;

  DISALLOW_COPY_AND_ASSIGN(NavigationContextImpl);
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_CONTEXT_IMPL_H_
