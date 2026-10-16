// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_METRICS_H_

#import <string_view>

#import "base/time/time.h"
#import "components/actor/public/mojom/actor_types.mojom-forward.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace actor {

// Returns the full histogram name for recording the execution duration of
// `call` (e.g. `IOS.Actor.Tools.JavaScriptExecutionDuration.Click`).
std::string_view GetJavaScriptExecutionDurationHistogramName(
    ActorJavaScriptCall call);

// Returns the full histogram name for recording the `mojom::ActionResultCode`
// of `call` (e.g. `IOS.Actor.Tools.JavaScriptResultCode.Click`).
std::string_view GetJavaScriptResultCodeHistogramName(ActorJavaScriptCall call);

// Records the execution duration of an Actor JavaScript call for `call`.
// Should only be called when the JavaScript execution completed successfully or
// returned an error payload (not on timeout or malformed payloads).
void RecordJavaScriptExecutionDuration(ActorJavaScriptCall call,
                                       base::TimeDelta duration);

// Records the `mojom::ActionResultCode` of an Actor JavaScript call for
// `call`.
void RecordJavaScriptResultCode(ActorJavaScriptCall call,
                                mojom::ActionResultCode code);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_PUBLIC_METRICS_H_
