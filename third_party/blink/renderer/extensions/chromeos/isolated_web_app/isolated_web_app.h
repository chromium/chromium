// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_ISOLATED_WEB_APP_ISOLATED_WEB_APP_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_ISOLATED_WEB_APP_ISOLATED_WEB_APP_H_

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class IsolatedWebApp : public ScriptWrappable,
                       public Supplement<ExecutionContext> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IsolatedWebApp& From(ExecutionContext&);

  static const char kSupplementName[];

  explicit IsolatedWebApp(ExecutionContext&);

  IsolatedWebApp(const IsolatedWebApp&) = delete;
  IsolatedWebApp& operator=(const IsolatedWebApp&) = delete;

  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_ISOLATED_WEB_APP_ISOLATED_WEB_APP_H_
