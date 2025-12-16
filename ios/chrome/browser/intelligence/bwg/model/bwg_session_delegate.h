// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_DELEGATE_H_

// BWGSessionDelegate is being renamed to GeminiSessionDelegate. All
// functionality has been moved to GeminiSessionDelegate. For backwards
// compatibility, gemini_session_delegate.h is imported and the protocol
// BWGSessionDelegate conforms to GeminiSessionDelegate.
// TODO(crbug.com/467341096): Remove this file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_delegate.h"

@protocol BWGSessionDelegate <GeminiSessionDelegate>
@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_DELEGATE_H_
