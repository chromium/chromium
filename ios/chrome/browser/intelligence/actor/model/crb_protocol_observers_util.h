// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_CRB_PROTOCOL_OBSERVERS_UTIL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_CRB_PROTOCOL_OBSERVERS_UTIL_H_

#import <Foundation/Foundation.h>

@class CRBProtocolObservers;

namespace actor {

// Returns whether `observer` is registered in `observers`.
bool ContainsObserver(CRBProtocolObservers* observers, id observer);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_CRB_PROTOCOL_OBSERVERS_UTIL_H_
