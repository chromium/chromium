// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLEDAPP_NAVIGATOR_INSTALLED_APP_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLEDAPP_NAVIGATOR_INSTALLED_APP_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Navigator;
class ScriptPromise;
class ScriptState;

class NavigatorInstalledApp final {
  STATIC_ONLY(NavigatorInstalledApp);

 public:
  static ScriptPromise getInstalledRelatedApps(ScriptState*, Navigator&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLEDAPP_NAVIGATOR_INSTALLED_APP_H_
