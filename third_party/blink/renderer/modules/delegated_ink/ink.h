// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_INK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_INK_H_

#include "third_party/blink/renderer/modules/delegated_ink/delegated_ink_trail_presenter.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Element;
class Navigator;
class ScriptPromise;
class ScriptState;

class Ink : public ScriptWrappable, public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static Ink* ink(Navigator& navigator);

  explicit Ink(Navigator&);
  ScriptPromise requestPresenter(ScriptState* state,
                                 String type,
                                 Element* presentationArea = nullptr);

  void Trace(blink::Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_INK_H_
