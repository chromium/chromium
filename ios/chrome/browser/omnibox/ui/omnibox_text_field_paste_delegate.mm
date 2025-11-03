// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_paste_delegate.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface OmniboxTextFieldPasteDelegate ()

@property(nonatomic, strong) NSURL* URL;

@end

@implementation OmniboxTextFieldPasteDelegate
@synthesize URL = _URL;
@synthesize textInput = _textInput;

- (void)textPasteConfigurationSupporting:
            (id<UITextPasteConfigurationSupporting>)
                textPasteConfigurationSupporting
                      transformPasteItem:(id<UITextPasteItem>)item {
  if ([item.itemProvider canLoadObjectOfClass:[NSURL class]]) {
    [item.itemProvider
        loadObjectOfClass:[NSURL class]
        completionHandler:^(id<NSItemProviderReading> object, NSError* error) {
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
  NSMutableDictionary<NSAttributedStringKey, id>* attributes =
      [[NSMutableDictionary alloc] init];
  UIFont* font = [self.textInput currentFont];
  if (font) {
    attributes[NSFontAttributeName] = font;
  }
  attributes[NSForegroundColorAttributeName] =
      [UIColor colorNamed:kTextPrimaryColor];
  attributes[NSBackgroundColorAttributeName] = [UIColor clearColor];

  // If there's a cached URL, use that. Otherwise, use one of the item strings.
  if (self.URL) {
    NSString* URLString = [self.URL absoluteString];
    self.URL = nil;
    return [[NSAttributedString alloc] initWithString:URLString
                                           attributes:attributes];
  } else {
    // Return only one item string to avoid repetition, for example when there
    // are both a URL and a string in the pasteboard.
    NSString* string = [itemStrings firstObject].string ?: @"";
    return [[NSAttributedString alloc] initWithString:string
                                           attributes:attributes];
  }
}

@end
