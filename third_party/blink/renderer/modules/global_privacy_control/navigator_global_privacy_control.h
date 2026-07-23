// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GLOBAL_PRIVACY_CONTROL_NAVIGATOR_GLOBAL_PRIVACY_CONTROL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GLOBAL_PRIVACY_CONTROL_NAVIGATOR_GLOBAL_PRIVACY_CONTROL_H_

namespace blink {
class NavigatorBase;
namespace NavigatorGlobalPrivacyControl {
bool globalPrivacyControl(NavigatorBase&);
}  // namespace NavigatorGlobalPrivacyControl
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GLOBAL_PRIVACY_CONTROL_NAVIGATOR_GLOBAL_PRIVACY_CONTROL_H_
