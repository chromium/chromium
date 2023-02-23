// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_selection/web_selection_response.h"

#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/shared_highlighting/ios/parsing_utils.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebSelectionResponse ()
- (instancetype)initWithSelectedText:(NSString*)selectedText
                          sourceView:(UIView*)sourceView
                          sourceRect:(CGRect)sourceRect
                               valid:(BOOL)valid NS_DESIGNATED_INITIALIZER;

- (instancetype)initInvalid;
@end

@implementation WebSelectionResponse

+ (instancetype)selectionResponseWithValue:(const base::Value&)value
                                  webState:(web::WebState*)webState {
  DCHECK(webState);

  const base::Value::Dict& dict = value.GetDict();
  const std::string* selectedText = dict.FindString("selectedText");
  absl::optional<CGRect> sourceRect =
      shared_highlighting::ParseRect(dict.Find("selectionRect"));

  // All values must be present to have a valid payload.
  if (!selectedText || !sourceRect) {
    return [WebSelectionResponse invalidResponse];
  }

  return [[WebSelectionResponse alloc]
      initWithSelectedText:base::SysUTF8ToNSString(*selectedText)
                sourceView:webState->GetView()
                sourceRect:shared_highlighting::ConvertToBrowserRect(
                               sourceRect.value(), webState)
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
