// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/frame/navigation_rate_limiter.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

NavigationRateLimiter::NavigationRateLimiter(Frame& frame)
    : frame_(frame),
      time_first_count_(base::TimeTicks::Now()),
      enabled(frame_->GetSettings()->GetShouldProtectAgainstIpcFlooding()) {}

void NavigationRateLimiter::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
}

bool NavigationRateLimiter::CanProceed() {
  if (!enabled)
    return true;

  // The aim is to roughly enable 20 same-document navigation per second, but we
  // express it as 200 per 10 seconds because some use cases (including tests)
  // do more than 20 updates in 1 second. But over time, applications shooting
  // for more should work. If necessary to support legitimate applications, we
  // can increase this threshold somewhat.
  static constexpr int kStateUpdateLimit = 200;
  static constexpr base::TimeDelta kStateUpdateLimitResetInterval =
      base::Seconds(10);

  if (++count_ <= kStateUpdateLimit)
    return true;

  const base::TimeTicks now = base::TimeTicks::Now();
  if (now - time_first_count_ > kStateUpdateLimitResetInterval) {
    time_first_count_ = now;
    count_ = 1;
    error_message_sent_ = false;
    return true;
  }

  // Display an error message. Do it only once in a while, else it will flood
  // the browser process with the DidAddMessageToConsole Mojo call.
  if (!error_message_sent_) {
    error_message_sent_ = true;
    if (auto* local_frame = DynamicTo<LocalFrame>(frame_.Get())) {
      local_frame->Console().AddMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kWarning,
          "Throttling navigation to prevent the browser from hanging. See "
          "https://crbug.com/1038223. Command line switch "
          "--disable-ipc-flooding-protection can be used to bypass the "
          "protection"));
    }
  }

  return false;
}

}  // namespace blink
