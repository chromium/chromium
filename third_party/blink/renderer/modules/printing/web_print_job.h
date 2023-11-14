// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PRINTING_WEB_PRINT_JOB_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PRINTING_WEB_PRINT_JOB_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExecutionContext;

class MODULES_EXPORT WebPrintJob : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit WebPrintJob(ExecutionContext* execution_context);
  ~WebPrintJob() override;

  void Trace(Visitor* visitor) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PRINTING_WEB_PRINT_JOB_H_
