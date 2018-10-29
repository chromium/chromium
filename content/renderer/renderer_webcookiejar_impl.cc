// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webcookiejar_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/frame_messages.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"

using blink::WebString;
using blink::WebURL;

namespace content {

void RendererWebCookieJarImpl::SetCookie(const WebURL& url,
                                         const WebURL& site_for_cookies,
                                         const WebString& value) {
  std::string value_utf8 =
      value.Utf8(WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);
  RenderThreadImpl::current()->render_frame_message_filter()->SetCookie(
      sender_->GetRoutingID(), url, site_for_cookies, value_utf8);
}

WebString RendererWebCookieJarImpl::Cookies(const WebURL& url,
                                            const WebURL& site_for_cookies) {
  std::string value_utf8;
  RenderThreadImpl::current()->render_frame_message_filter()->GetCookies(
      sender_->GetRoutingID(), url, site_for_cookies, &value_utf8);
  return WebString::FromUTF8(value_utf8);
}

bool RendererWebCookieJarImpl::CookiesEnabled(const WebURL& url,
                                              const WebURL& site_for_cookies) {
  bool cookies_enabled = false;
  sender_->Send(new FrameHostMsg_CookiesEnabled(
      sender_->GetRoutingID(), url, site_for_cookies, &cookies_enabled));
  return cookies_enabled;
}

}  // namespace content
