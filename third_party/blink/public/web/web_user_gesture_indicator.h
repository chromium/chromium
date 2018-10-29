/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_USER_GESTURE_INDICATOR_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_USER_GESTURE_INDICATOR_H_

#include "third_party/blink/public/common/frame/user_activation_update_source.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class WebLocalFrame;
class WebUserGestureToken;

class WebUserGestureIndicator {
 public:
  // Returns true if a user gesture is currently being processed. Must be called
  // on the main thread.
  BLINK_EXPORT static bool IsProcessingUserGesture(WebLocalFrame*);

  // Can be called from any thread. Note that this is slower than the non
  // thread-safe version due to thread id lookups. Prefer the non thread-safe
  // version for code that will only execute on the main thread.
  BLINK_EXPORT static bool IsProcessingUserGestureThreadSafe(WebLocalFrame*);

  // Returns true if a consumable gesture exists and has been successfully
  // consumed.
  BLINK_EXPORT static bool ConsumeUserGesture(
      WebLocalFrame*,
      UserActivationUpdateSource update_source =
          UserActivationUpdateSource::kRenderer);

  // Returns true if a user gesture was processed on the provided frame since
  // the time the frame was loaded.
  BLINK_EXPORT static bool ProcessedUserGestureSinceLoad(WebLocalFrame*);

  // Returns a token for the currently active user gesture. It can be used to
  // continue processing the user gesture later on using a
  // WebScopedUserGesture.
  BLINK_EXPORT static WebUserGestureToken CurrentUserGestureToken();

  BLINK_EXPORT static void ExtendTimeout();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_USER_GESTURE_INDICATOR_H_
