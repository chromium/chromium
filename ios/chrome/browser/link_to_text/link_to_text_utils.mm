// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_utils.h"

#import "base/time/time.h"
#import "base/values.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "ios/chrome/browser/link_to_text/link_to_text_constants.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using shared_highlighting::LinkGenerationError;

namespace link_to_text {

namespace {
const CGFloat kCaretWidth = 4.0;
}  // namespace

BOOL IsValidDictValue(const base::Value* value) {
  return value && value->is_dict() && !value->DictEmpty();
}

base::Optional<LinkGenerationOutcome> ParseStatus(
    base::Optional<double> status) {
  if (!status.has_value()) {
    return base::nullopt;
  }

  int status_value = static_cast<int>(status.value());
  if (status_value < 0 ||
      status_value > static_cast<int>(LinkGenerationOutcome::kMaxValue)) {
    return base::nullopt;
  }

  return static_cast<LinkGenerationOutcome>(status_value);
}

shared_highlighting::LinkGenerationError OutcomeToError(
    LinkGenerationOutcome outcome) {
  switch (outcome) {
    case LinkGenerationOutcome::kInvalidSelection:
      return LinkGenerationError::kIncorrectSelector;
      break;
    case LinkGenerationOutcome::kAmbiguous:
      return LinkGenerationError::kContextExhausted;
      break;
    case LinkGenerationOutcome::kTimeout:
      return LinkGenerationError::kTimeout;
      break;
    case LinkGenerationOutcome::kSuccess:
      // kSuccess is not supposed to happen, as it is not an error.
      NOTREACHED();
      return LinkGenerationError::kUnknown;
      break;
  }
}

base::Optional<CGRect> ParseRect(const base::Value* value) {
  if (!IsValidDictValue(value)) {
    return base::nullopt;
  }

  const base::Value* xValue =
      value->FindKeyOfType("x", base::Value::Type::DOUBLE);
  const base::Value* yValue =
      value->FindKeyOfType("y", base::Value::Type::DOUBLE);
  const base::Value* widthValue =
      value->FindKeyOfType("width", base::Value::Type::DOUBLE);
  const base::Value* heightValue =
      value->FindKeyOfType("height", base::Value::Type::DOUBLE);

  if (!xValue || !yValue || !widthValue || !heightValue) {
    return base::nullopt;
  }

  return CGRectMake(xValue->GetDouble(), yValue->GetDouble(),
                    widthValue->GetDouble(), heightValue->GetDouble());
}

base::Optional<GURL> ParseURL(const std::string* url_value) {
  if (!url_value) {
    return base::nullopt;
  }

  GURL url(*url_value);
  if (!url.is_empty() && url.is_valid()) {
    return url;
  }

  return base::nullopt;
}

CGRect ConvertToBrowserRect(CGRect web_view_rect, web::WebState* web_state) {
  if (CGRectEqualToRect(web_view_rect, CGRectZero) || !web_state) {
    return web_view_rect;
  }

  id<CRWWebViewProxy> web_view_proxy = web_state->GetWebViewProxy();
  CGFloat zoom_scale = web_view_proxy.scrollViewProxy.zoomScale;
  UIEdgeInsets inset = web_view_proxy.scrollViewProxy.contentInset;

  return CGRectMake((web_view_rect.origin.x * zoom_scale) + inset.left,
                    (web_view_rect.origin.y * zoom_scale) + inset.top,
                    (web_view_rect.size.width * zoom_scale) + kCaretWidth,
                    web_view_rect.size.height * zoom_scale);
}

BOOL IsLinkGenerationTimeout(base::TimeDelta latency) {
  return latency.InMilliseconds() >= kLinkGenerationTimeoutInMs;
}

}  // namespace link_to_text
