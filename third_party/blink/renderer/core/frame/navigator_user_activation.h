// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_USER_ACTIVATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_USER_ACTIVATION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class UserActivation;

class CORE_EXPORT NavigatorUserActivation final
    : public GarbageCollected<NavigatorUserActivation> {
 public:
  static UserActivation* userActivation(Navigator& navigator);
  UserActivation* userActivation();

  explicit NavigatorUserActivation(Navigator&);

  void Trace(Visitor*) const;

 private:
  static NavigatorUserActivation& From(Navigator&);

  Member<UserActivation> user_activation_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_NAVIGATOR_USER_ACTIVATION_H_
