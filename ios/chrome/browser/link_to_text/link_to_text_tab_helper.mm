// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_tab_helper.h"

#import "base/bind.h"
#import "base/optional.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const double kJavaScriptFunctionCallTimeoutMs = 200.0;
const CGFloat kCaretWidth = 4.0;
const char kGetLinkToTextJavaScript[] = "textFragments.getLinkToText";

// Attempts to convert the given string pointer |value| into a valid GURL. If
// |value| does not represent a valid URL, then the returned base::Optional
// instance will be empty.
base::Optional<GURL> ParseURL(const std::string* value) {
  if (!value) {
    return base::nullopt;
  }

  base::Optional<GURL> url = base::make_optional<GURL>(*value);
  return url->is_valid() ? url : base::nullopt;
}

// Attempts to parse the given |value| into a CGRect. If |value| does not map
// into the expected structure, an empty base::Optional instance will be
// returned.
base::Optional<CGRect> ParseRect(const base::Value* value) {
  if (!value || !value->is_dict()) {
    return base::nullopt;
  }

  const base::Value* x_value =
      value->FindKeyOfType("x", base::Value::Type::DOUBLE);
  const base::Value* y_value =
      value->FindKeyOfType("y", base::Value::Type::DOUBLE);
  const base::Value* width_value =
      value->FindKeyOfType("width", base::Value::Type::DOUBLE);
  const base::Value* height_value =
      value->FindKeyOfType("height", base::Value::Type::DOUBLE);

  if (!x_value || !y_value || !width_value || !height_value) {
    return base::nullopt;
  }

  return CGRectMake(x_value->GetDouble(), y_value->GetDouble(),
                    width_value->GetDouble(), height_value->GetDouble());
}

}  // namespace

LinkToTextTabHelper::LinkToTextTabHelper(web::WebState* web_state)
    : web_state_(web_state), weak_ptr_factory_(this) {
  web_state_->AddObserver(this);
  web_view_proxy_ = web_state_->GetWebViewProxy();
}

LinkToTextTabHelper::~LinkToTextTabHelper() {}

// static
void LinkToTextTabHelper::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(), base::WrapUnique(new LinkToTextTabHelper(web_state)));
  }
}

bool LinkToTextTabHelper::ShouldOffer() {
  // TODO(crbug.com/1134708): add more checks, like text only.
  return true;
}

void LinkToTextTabHelper::GetLinkToText(LinkToTextCallback callback) {
  base::WeakPtr<LinkToTextTabHelper> weak_ptr = weak_ptr_factory_.GetWeakPtr();
  web_state_->GetWebFramesManager()->GetMainWebFrame()->CallJavaScriptFunction(
      kGetLinkToTextJavaScript, {},
      base::BindOnce(^(const base::Value* response) {
        if (weak_ptr) {
          weak_ptr->OnJavaScriptResponseReceived(callback, response);
        }
      }),
      base::TimeDelta::FromMilliseconds(kJavaScriptFunctionCallTimeoutMs));
}

void LinkToTextTabHelper::OnJavaScriptResponseReceived(
    LinkToTextCallback callback,
    const base::Value* response) {
  LinkToTextPayload* payload = ParseResponse(response);

  if (!payload) {
    return;
  }

  callback(payload);
}

LinkToTextPayload* LinkToTextTabHelper::ParseResponse(
    const base::Value* response) {
  if (!response || !web_state_) {
    return nil;
  }

  NSString* title = tab_util::GetTabTitle(web_state_);
  base::Optional<GURL> link = ParseURL(response->FindStringKey("link"));
  const std::string* selected_text = response->FindStringKey("selectedText");
  base::Optional<CGRect> source_rect =
      ParseRect(response->FindKey("selectionRect"));

  // All values must be present to have a valid payload.
  if (!title || !link || !selected_text || !source_rect) {
    return nil;
  }

  return [[LinkToTextPayload alloc]
       initWithURL:link.value()
             title:title
      selectedText:base::SysUTF8ToNSString(*selected_text)
        sourceView:web_state_->GetView()
        sourceRect:ConvertToBrowserRect(source_rect.value())];
}

CGRect LinkToTextTabHelper::ConvertToBrowserRect(CGRect web_view_rect) {
  if (CGRectEqualToRect(web_view_rect, CGRectZero)) {
    return web_view_rect;
  }

  CGFloat zoom_scale = web_view_proxy_.scrollViewProxy.zoomScale;
  UIEdgeInsets inset = web_view_proxy_.scrollViewProxy.contentInset;
  return CGRectMake((web_view_rect.origin.x * zoom_scale) + inset.left,
                    (web_view_rect.origin.y * zoom_scale) + inset.top,
                    (web_view_rect.size.width * zoom_scale) + kCaretWidth,
                    web_view_rect.size.height * zoom_scale);
}

void LinkToTextTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);

  web_state_->RemoveObserver(this);
  web_state_ = nil;

  // The call to RemoveUserData cause the destruction of the current instance,
  // so nothing should be done after that point (this is like "delete this;").
  web_state->RemoveUserData(UserDataKey());
}

WEB_STATE_USER_DATA_KEY_IMPL(LinkToTextTabHelper)
