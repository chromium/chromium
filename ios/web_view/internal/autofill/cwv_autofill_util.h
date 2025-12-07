// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_UTIL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_UTIL_H_

#import <Foundation/Foundation.h>

#include <vector>

namespace autofill {
class LegalMessageLine;
using LegalMessageLines = std::vector<LegalMessageLine>;
}  // namespace autofill

NS_ASSUME_NONNULL_BEGIN

// Converts |autofill::LegalMessageLines| into an array of attributed strings
// suitable for display in the UI. Links in the original message are converted
// to `NSLinkAttributeName` attributes.
NSArray<NSAttributedString*>* CWVLegalMessagesFromLegalMessageLines(
    const autofill::LegalMessageLines& legalMessageLines);

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_UTIL_H_
