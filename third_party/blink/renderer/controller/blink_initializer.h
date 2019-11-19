// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLINK_INITIALIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLINK_INITIALIZER_H_

#include "third_party/blink/renderer/modules/modules_initializer.h"

namespace blink {

class BlinkInitializer : public ModulesInitializer {
 public:
  void RegisterInterfaces(mojo::BinderMap&) override;
  void OnClearWindowObjectInMainWorld(Document&,
                                      const Settings&) const override;
  void InitLocalFrame(LocalFrame&) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_BLINK_INITIALIZER_H_
