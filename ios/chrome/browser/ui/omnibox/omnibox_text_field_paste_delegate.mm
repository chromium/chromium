// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_paste_delegate.h"

#import "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxTextFieldPasteDelegate ()

@property(nonatomic, strong) NSURL* URL;

@end

@implementation OmniboxTextFieldPasteDelegate
@synthesize URL = _URL;

- (void)textPasteConfigurationSupporting:
    (id<UITextPasteConfigurationSupporting>)textPasteConfigurationSupporting
    transformPasteItem:(id<UITextPasteItem>)item
    API_AVAILABLE(ios(11.0)) {
  if ([item.itemProvider canLoadObjectOfClass:[NSURL class]]) {
    [item.itemProvider
        loadObjectOfClass:[NSURL class]
        completionHandler:^(id<NSItemProviderReading> _Nullable object,
                            NSError* _Nullable error) {
          if (!error) {
            self.URL = base::mac::ObjCCast<NSURL>(object);
          }
          [item setDefaultResult];
        }];
  } else {
    [item setDefaultResult];
  }
}

- (NSAttributedString*)textPasteConfigurationSupporting:
    (id<UITextPasteConfigurationSupporting>)textPasteConfigurationSupporting
    combineItemAttributedStrings:(NSArray<NSAttributedString*>*)itemStrings
                        forRange:(UITextRange*)textRange
    API_AVAILABLE(ios(11.0)) {
  // If there's a cached URL, use that. Otherwise, use one of the item strings.
  if (self.URL) {
    NSString* URLString = [self.URL absoluteString];
    self.URL = nil;
    return [[NSAttributedString alloc] initWithString:URLString];
  } else {
    // Return only one item string to avoid repetition, for example when there
    // are both a URL and a string in the pasteboard.
    NSAttributedString* string = [itemStrings firstObject];
    if (!string) {
      string = [[NSAttributedString alloc] init];
    }
    return string;
  }
}

@end
