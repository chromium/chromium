// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_util.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "net/base/apple/url_conversions.h"
#import "ui/gfx/range/range.h"

NS_ASSUME_NONNULL_BEGIN

NSArray<NSAttributedString*>* CWVLegalMessagesFromLegalMessageLines(
    const autofill::LegalMessageLines& legalMessageLines) {
  NSMutableArray<NSAttributedString*>* legalMessages = [NSMutableArray array];
  for (const autofill::LegalMessageLine& legalMessageLine : legalMessageLines) {
    NSString* text = base::SysUTF16ToNSString(legalMessageLine.text());
    NSMutableAttributedString* legalMessage =
        [[NSMutableAttributedString alloc] initWithString:text];
    for (const autofill::LegalMessageLine::Link& link :
         legalMessageLine.links()) {
      NSURL* url = net::NSURLWithGURL(link.url);
      NSRange range = link.range.ToNSRange();
      [legalMessage addAttribute:NSLinkAttributeName value:url range:range];
    }
    [legalMessages addObject:[legalMessage copy]];
  }
  return [legalMessages copy];
}

NS_ASSUME_NONNULL_END
