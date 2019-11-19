// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LOCKS_NAVIGATOR_LOCKS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LOCKS_NAVIGATOR_LOCKS_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LockManager;
class Navigator;
class ScriptState;
class WorkerNavigator;

class NavigatorLocks final {
  STATIC_ONLY(NavigatorLocks);

 public:
  static LockManager* locks(ScriptState*, Navigator&);
  static LockManager* locks(ScriptState*, WorkerNavigator&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LOCKS_NAVIGATOR_LOCKS_H_
