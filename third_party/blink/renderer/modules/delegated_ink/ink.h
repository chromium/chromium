// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_INK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_INK_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
class DelegatedInkTrailPresenter;
class InkPresenterParam;
class Navigator;
class ScriptState;

class Ink : public ScriptWrappable, public GarbageCollectedMixin {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Ink* ink(Navigator& navigator);

  explicit Ink(Navigator&);
  ScriptPromise<DelegatedInkTrailPresenter> requestPresenter(
      ScriptState* state,
      InkPresenterParam* presenter_param);

  void Trace(blink::Visitor*) const override;

 private:
  Member<Navigator> navigator_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_INK_H_
