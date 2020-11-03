// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/link_to_text_utils.h"

#import "base/values.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
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

CGRect ConvertToBrowserRect(CGRect webViewRect, web::WebState* webState) {
  if (CGRectEqualToRect(webViewRect, CGRectZero) || !webState) {
    return webViewRect;
  }

  id<CRWWebViewProxy> webViewProxy = webState->GetWebViewProxy();
  CGFloat zoomScale = webViewProxy.scrollViewProxy.zoomScale;
  UIEdgeInsets inset = webViewProxy.scrollViewProxy.contentInset;

  return CGRectMake((webViewRect.origin.x * zoomScale) + inset.left,
                    (webViewRect.origin.y * zoomScale) + inset.top,
                    (webViewRect.size.width * zoomScale) + kCaretWidth,
                    webViewRect.size.height * zoomScale);
}
}  // namespace link_to_text
