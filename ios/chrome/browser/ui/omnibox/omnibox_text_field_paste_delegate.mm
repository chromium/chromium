// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_paste_delegate.h"

#import "base/apple/foundation_util.h"

@interface OmniboxTextFieldPasteDelegate ()

@property(nonatomic, strong) NSURL* URL;

@end

@implementation OmniboxTextFieldPasteDelegate
@synthesize URL = _URL;

- (void)textPasteConfigurationSupporting:
            (id<UITextPasteConfigurationSupporting>)
                textPasteConfigurationSupporting
                      transformPasteItem:(id<UITextPasteItem>)item {
  if ([item.itemProvider canLoadObjectOfClass:[NSURL class]]) {
    [item.itemProvider
        loadObjectOfClass:[NSURL class]
        completionHandler:^(id<NSItemProviderReading> _Nullable object,
                            NSError* _Nullable error) {
          if (!error) {
            self.URL = base::apple::ObjCCast<NSURL>(object);
          }
          [item setDefaultResult];
        }];
  } else {
    [item setDefaultResult];
  }
}

- (NSAttributedString*)
    textPasteConfigurationSupporting:
        (id<UITextPasteConfigurationSupporting>)textPasteConfigurationSupporting
        combineItemAttributedStrings:(NSArray<NSAttributedString*>*)itemStrings
                            forRange:(UITextRange*)textRange {
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
