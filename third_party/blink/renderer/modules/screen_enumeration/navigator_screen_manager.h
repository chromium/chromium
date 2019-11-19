// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_NAVIGATOR_SCREEN_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_NAVIGATOR_SCREEN_MANAGER_H_

#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class Navigator;
class ScreenManager;
class ScriptState;
class WorkerNavigator;

// Exposes the ScreenManager interface on both the Navigator and
// WorkerNavigator interfaces.
class MODULES_EXPORT NavigatorScreenManager {
 public:
  // The ScreenManager exposed in the Navigator execution context.
  static ScreenManager* screen(Navigator&);

  // The ScreenManager exposed in the WorkerNavigator execution context.
  static ScreenManager* screen(ScriptState*, WorkerNavigator&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_NAVIGATOR_SCREEN_MANAGER_H_
