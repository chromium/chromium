// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_selection/model/web_selection_response.h"

#import "base/strings/sys_string_conversions.h"
#import "components/shared_highlighting/ios/parsing_utils.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

namespace {

CGRect ConvertToBrowserRect(CGRect web_view_rect, web::WebState* web_state) {
  if (CGRectEqualToRect(web_view_rect, CGRectZero) || !web_state) {
    return web_view_rect;
  }

  id<CRWWebViewProxy> web_view_proxy = web_state->GetWebViewProxy();
  CGFloat zoom_scale = web_view_proxy.scrollViewProxy.zoomScale;

  return CGRectMake((web_view_rect.origin.x * zoom_scale),
                    (web_view_rect.origin.y * zoom_scale),
                    (web_view_rect.size.width * zoom_scale),
                    web_view_rect.size.height * zoom_scale);
}

}  // namespace

@interface WebSelectionResponse ()
- (instancetype)initWithSelectedText:(NSString*)selectedText
                          sourceView:(UIView*)sourceView
                          sourceRect:(CGRect)sourceRect
                               valid:(BOOL)valid NS_DESIGNATED_INITIALIZER;

- (instancetype)initInvalid;
@end

@implementation WebSelectionResponse

+ (instancetype)selectionResponseWithDict:(const base::Value::Dict&)dict
                                 webState:(web::WebState*)webState {
  DCHECK(webState);

  const std::string* selectedText = dict.FindString("selectedText");
  std::optional<CGRect> sourceRect =
      shared_highlighting::ParseRect(dict.FindDict("selectionRect"));

  // All values must be present to have a valid payload.
  if (!selectedText || !sourceRect) {
    return [WebSelectionResponse invalidResponse];
  }

  return [[WebSelectionResponse alloc]
      initWithSelectedText:base::SysUTF8ToNSString(*selectedText)
                sourceView:webState->GetView()
                sourceRect:ConvertToBrowserRect(sourceRect.value(), webState)
                     valid:YES];
}

+ (instancetype)invalidResponse {
  return [[WebSelectionResponse alloc] initInvalid];
}

- (instancetype)initWithSelectedText:(NSString*)selectedText
                          sourceView:(UIView*)sourceView
                          sourceRect:(CGRect)sourceRect
                               valid:(BOOL)valid {
  self = [super init];
  if (self) {
    _valid = valid;
    _selectedText = selectedText;
    _sourceView = sourceView;
    _sourceRect = sourceRect;
  }
  return self;
}

- (instancetype)initInvalid {
  self = [self initWithSelectedText:nil
                         sourceView:nil
                         sourceRect:CGRectZero
                              valid:NO];
  return self;
}

@end
