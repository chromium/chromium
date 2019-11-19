// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_navigation_context.h"

#import "ios/web/public/web_state.h"
#include "net/http/http_response_headers.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

// Returns a new unique ID for a NavigationContext during construction.
// The returned ID is guaranteed to be nonzero (zero is the "no ID" indicator).
int64_t CreateUniqueContextId() {
  static int64_t unique_id_counter = 0;
  return ++unique_id_counter;
}

}  // namespace

FakeNavigationContext::FakeNavigationContext()
    : navigation_id_(CreateUniqueContextId()) {}
FakeNavigationContext::~FakeNavigationContext() = default;

WebState* FakeNavigationContext::GetWebState() {
  return web_state_.get();
}

int64_t FakeNavigationContext::GetNavigationId() const {
  return navigation_id_;
}

const GURL& FakeNavigationContext::GetUrl() const {
  return url_;
}

bool FakeNavigationContext::HasUserGesture() const {
  return has_user_gesture_;
}

ui::PageTransition FakeNavigationContext::GetPageTransition() const {
  return page_transition_;
}

bool FakeNavigationContext::IsSameDocument() const {
  return same_document_;
}

bool FakeNavigationContext::HasCommitted() const {
  return has_committed_;
}

bool FakeNavigationContext::IsDownload() const {
  return is_download_;
}

bool FakeNavigationContext::IsPost() const {
  return is_post_;
}

NSError* FakeNavigationContext::GetError() const {
  return error_;
}

net::HttpResponseHeaders* FakeNavigationContext::GetResponseHeaders() const {
  return response_headers_.get();
}

bool FakeNavigationContext::IsRendererInitiated() const {
  return renderer_initiated_;
}

void FakeNavigationContext::SetWebState(std::unique_ptr<WebState> web_state) {
  web_state_ = std::move(web_state);
}

void FakeNavigationContext::SetUrl(const GURL& url) {
  url_ = url;
}

void FakeNavigationContext::SetHasUserGesture(bool has_user_gesture) {
  has_user_gesture_ = has_user_gesture;
}

void FakeNavigationContext::SetPageTransition(ui::PageTransition transition) {
  page_transition_ = transition;
}

void FakeNavigationContext::SetIsSameDocument(bool same_document) {
  same_document_ = same_document;
}

void FakeNavigationContext::SetHasCommitted(bool has_committed) {
  has_committed_ = has_committed;
}

void FakeNavigationContext::SetIsDownload(bool is_download) {
  is_download_ = is_download;
}

void FakeNavigationContext::SetIsPost(bool is_post) {
  is_post_ = is_post;
}

void FakeNavigationContext::SetError(NSError* error) {
  error_ = error;
}

void FakeNavigationContext::SetResponseHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& response_headers) {
  response_headers_ = response_headers;
}

void FakeNavigationContext::SetIsRendererInitiated(bool renderer_initiated) {
  renderer_initiated_ = renderer_initiated;
}

}  // namespace web
