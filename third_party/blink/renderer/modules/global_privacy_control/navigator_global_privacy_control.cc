// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/global_privacy_control/navigator_global_privacy_control.h"

#include "third_party/blink/public/common/global_privacy_control/global_privacy_control_util.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"

namespace blink {
namespace NavigatorGlobalPrivacyControl {

bool globalPrivacyControl(NavigatorBase& navigator) {
  if (navigator.DomWindow()) {
    return IsGlobalPrivacyControlFeatureAndSettingEnabled(
        navigator.DomWindow()->GetFrame()->GetPage()->GetRendererPreferences());
  } else if (WorkerOrWorkletGlobalScope* worker_scope =
                 DynamicTo<WorkerOrWorkletGlobalScope>(
                     navigator.GetExecutionContext())) {
    return IsGlobalPrivacyControlFeatureAndSettingEnabled(
        worker_scope->GetRendererPreferences());
  }
  return false;
}

}  // namespace NavigatorGlobalPrivacyControl
}  // namespace blink
