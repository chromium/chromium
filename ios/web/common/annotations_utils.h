// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_ANNOTATIONS_UTILS_H_
#define IOS_WEB_COMMON_ANNOTATIONS_UTILS_H_

#import <UIKit/UIKit.h>

#import "base/values.h"

namespace web {

using TextAnnotation = std::pair<base::Value::Dict, NSTextCheckingResult*>;

// Checks if the detected entity is an URL and more specifically an email.
bool IsNSTextCheckingResultEmail(NSTextCheckingResult* result);

// Returns a NSTextCheckingTypeLink result from an email string.
NSTextCheckingResult* MakeNSTextCheckingResultEmail(NSString* email,
                                                    NSRange range);

// Encapsulates data into a `TextAnnotation` that can be passed to JS.
TextAnnotation ConvertMatchToAnnotation(NSString* source,
                                        NSRange range,
                                        NSTextCheckingResult* data,
                                        NSString* type);

}  // namespace web

#endif  // IOS_WEB_COMMON_ANNOTATIONS_UTILS_H_
