// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SUBAPPS_SUB_APPS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SUBAPPS_SUB_APPS_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class Navigator;
class ScriptPromise;
class ScriptState;

class SubApps : public ScriptWrappable, public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static SubApps* subApps(Navigator&);

  explicit SubApps(Navigator&);
  SubApps(const SubApps&) = delete;
  SubApps& operator=(const SubApps&) = delete;

  // ScriptWrappable.
  void Trace(Visitor*) const override;

  // SubApps API.
  ScriptPromise add(ScriptState*, const String&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SUBAPPS_SUB_APPS_H_
