// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_USER_ACTIVATION_UPDATE_TYPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_USER_ACTIVATION_UPDATE_TYPE_H_

namespace blink {

// Types of UserActivationV2 state updates sent between the browser and the
// renderer processes.
enum class UserActivationUpdateType {
  kNotifyActivation,
  kNotifyActivationPendingBrowserVerification,
  kConsumeTransientActivation,
  kClearActivation,
  kMaxValue = kClearActivation
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_USER_ACTIVATION_UPDATE_TYPE_H_
