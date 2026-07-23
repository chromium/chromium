// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/global_privacy_control/navigator_global_privacy_control.h"

#include "third_party/blink/public/common/global_privacy_control/global_privacy_control_util.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"

namespace blink {
namespace NavigatorGlobalPrivacyControl {

bool globalPrivacyControl(NavigatorBase& navigator) {
  // TODO(crbug.com/40745270): Currently, the GPC signal is controlled by a
  // feature flag, when a user facing setting is added, this should be modified
  // to use frame cached value.
  return IsGlobalPrivacyControlEnabled();
}

}  // namespace NavigatorGlobalPrivacyControl
}  // namespace blink
