// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_SUPPLEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_SUPPLEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script_tools/model_context.h"
#include "third_party/blink/renderer/core/script_tools/model_context_testing.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;

class CORE_EXPORT ModelContextSupplement final
    : public GarbageCollected<ModelContextSupplement>,
      public Supplement<Navigator> {
 public:
  static const char kSupplementName[];

  static ModelContextSupplement& From(Navigator&);
  static ModelContext* GetIfExists(Navigator&);
  static ModelContext* modelContext(Navigator&);
  static ModelContextTesting* modelContextTesting(Navigator&);

  explicit ModelContextSupplement(Navigator&);
  ModelContextSupplement(const ModelContextSupplement&) = delete;
  ModelContextSupplement& operator=(const ModelContextSupplement&) = delete;

  void Trace(Visitor*) const override;

 private:
  ModelContext* modelContext();
  ModelContextTesting* modelContextTesting();

  Member<ModelContext> model_context_;
  Member<ModelContextTesting> model_context_testing_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_TOOLS_MODEL_CONTEXT_SUPPLEMENT_H_
