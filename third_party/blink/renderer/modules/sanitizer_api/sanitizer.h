// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DocumentFragment;
class ExceptionState;
class ScriptState;

class MODULES_EXPORT Sanitizer final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Sanitizer* Create(ExceptionState&);
  Sanitizer();
  ~Sanitizer() override;

  String sanitizeToString(const String&);

  DocumentFragment* sanitize(ScriptState*, const String&, ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_
