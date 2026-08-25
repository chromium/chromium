// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_MESSAGE_AUTOFILL_LEGAL_MESSAGE_LINE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_MESSAGE_AUTOFILL_LEGAL_MESSAGE_LINE_H_

#import <UIKit/UIKit.h>

#include <vector>

class GURL;

namespace autofill {
class LegalMessageLine;
}  // namespace autofill

// Represents a message with optional links. Each linkRange in `linkRanges`
// represents the range (in `messageText`) for the corresponding (same index)
// linkURL in `linkURLS`.
@interface AutofillLegalMessageLine : NSObject

@property(nonatomic, copy) NSString* messageText;

// TODO(crbug.com/413049626): Replace `linkURLs` and `linkRanges` with a single
// vector of (GURL, NSRange) pairs.
@property(nonatomic, strong) NSArray* linkRanges;

@property(nonatomic, assign) std::vector<GURL> linkURLs;

// Convert the C++ legal message lines to an NSArray of
// AutofillLegalMessageLine objects.
+ (NSMutableArray<AutofillLegalMessageLine*>*)convertFrom:
    (const std::vector<autofill::LegalMessageLine>&)autofillLegalMessageLines;
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_MESSAGE_AUTOFILL_LEGAL_MESSAGE_LINE_H_
