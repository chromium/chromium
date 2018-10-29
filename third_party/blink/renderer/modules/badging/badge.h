// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BADGING_BADGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BADGING_BADGE_H_

#include "third_party/blink/public/platform/modules/badging/badging.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptState;
class USVStringOrLong;

class Badge final : public ScriptWrappable,
                    public Supplement<ExecutionContext> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Badge);

 public:
  static const char kSupplementName[];

  static Badge* From(ExecutionContext*);

  ~Badge() override;

  // Badge IDL interface.
  static void set(ScriptState*, ExceptionState&);
  static void set(ScriptState*, USVStringOrLong&, ExceptionState&);
  static void clear(ScriptState*);

  void Set(USVStringOrLong*, ExceptionState&);
  void Clear();

  void Trace(blink::Visitor*) override;

 private:
  explicit Badge(ExecutionContext*);

  static Badge* BadgeFromState(ScriptState* script_state);

  blink::mojom::blink::BadgeServicePtr badge_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BADGING_BADGE_H_
