// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PRINTING_WEB_PRINT_JOB_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PRINTING_WEB_PRINT_JOB_H_

#include "third_party/blink/public/mojom/printing/web_printing.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ExecutionContext;
class WebPrintJobAttributes;

class MODULES_EXPORT WebPrintJob : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  WebPrintJob(ExecutionContext* execution_context,
              mojom::blink::WebPrintJobInfoPtr print_job_info);
  ~WebPrintJob() override;

  WebPrintJobAttributes* attributes() const { return attributes_; }

  void Trace(Visitor* visitor) const override;

 private:
  Member<WebPrintJobAttributes> attributes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PRINTING_WEB_PRINT_JOB_H_
