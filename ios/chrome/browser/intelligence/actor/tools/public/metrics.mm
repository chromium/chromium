// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/public/metrics.h"

#import <utility>

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace actor {

// LINT.IfChange(IOSActorJavaScriptCall)
std::string_view GetJavaScriptExecutionDurationHistogramName(
    ActorJavaScriptCall call) {
  switch (call) {
    case ActorJavaScriptCall::kActionTarget:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.ActionTarget";
    case ActorJavaScriptCall::kAttemptFormFilling:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.AttemptFormFilling";
    case ActorJavaScriptCall::kClick:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.Click";
    case ActorJavaScriptCall::kDragAndRelease:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.DragAndRelease";
    case ActorJavaScriptCall::kScroll:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.Scroll";
    case ActorJavaScriptCall::kScrollTo:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.ScrollTo";
    case ActorJavaScriptCall::kSelect:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.Select";
    case ActorJavaScriptCall::kType:
      return "IOS.Actor.Tools.JavaScriptExecutionDuration.Type";
  }
  NOTREACHED();
}

std::string_view GetJavaScriptResultCodeHistogramName(
    ActorJavaScriptCall call) {
  switch (call) {
    case ActorJavaScriptCall::kActionTarget:
      return "IOS.Actor.Tools.JavaScriptResultCode.ActionTarget";
    case ActorJavaScriptCall::kAttemptFormFilling:
      return "IOS.Actor.Tools.JavaScriptResultCode.AttemptFormFilling";
    case ActorJavaScriptCall::kClick:
      return "IOS.Actor.Tools.JavaScriptResultCode.Click";
    case ActorJavaScriptCall::kDragAndRelease:
      return "IOS.Actor.Tools.JavaScriptResultCode.DragAndRelease";
    case ActorJavaScriptCall::kScroll:
      return "IOS.Actor.Tools.JavaScriptResultCode.Scroll";
    case ActorJavaScriptCall::kScrollTo:
      return "IOS.Actor.Tools.JavaScriptResultCode.ScrollTo";
    case ActorJavaScriptCall::kSelect:
      return "IOS.Actor.Tools.JavaScriptResultCode.Select";
    case ActorJavaScriptCall::kType:
      return "IOS.Actor.Tools.JavaScriptResultCode.Type";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/histograms.xml:IOSActorJavaScriptCall,//ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h:IOSActorJavaScriptCall)

void RecordJavaScriptExecutionDuration(ActorJavaScriptCall call,
                                       base::TimeDelta duration) {
  base::UmaHistogramTimes(GetJavaScriptExecutionDurationHistogramName(call),
                          duration);
}

void RecordJavaScriptResultCode(ActorJavaScriptCall call,
                                mojom::ActionResultCode code) {
  base::UmaHistogramSparse(GetJavaScriptResultCodeHistogramName(call),
                           std::to_underlying(code));
}

}  // namespace actor
