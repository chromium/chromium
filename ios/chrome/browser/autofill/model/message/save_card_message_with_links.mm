// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "components/strings/grit/components_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#import "url/gurl.h"

@implementation SaveCardMessageWithLinks

+ (NSMutableArray<SaveCardMessageWithLinks*>*)convertFrom:
    (const autofill::LegalMessageLines&)autofillLegalMessageLines {
  NSMutableArray<SaveCardMessageWithLinks*>* legalMessages =
      [[NSMutableArray alloc] init];
  for (const auto& line : autofillLegalMessageLines) {
    SaveCardMessageWithLinks* message = [[SaveCardMessageWithLinks alloc] init];
    message.messageText = base::SysUTF16ToNSString(line.text());
    NSMutableArray* linkRanges = [[NSMutableArray alloc] init];
    std::vector<GURL> linkURLs;
    for (const auto& link : line.links()) {
      [linkRanges addObject:[NSValue valueWithRange:link.range.ToNSRange()]];
      linkURLs.push_back(link.url);
    }
    message.linkRanges = linkRanges;
    message.linkURLs = linkURLs;
    [legalMessages addObject:message];
  }
  return legalMessages;
}

@end
