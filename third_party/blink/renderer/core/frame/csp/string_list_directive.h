// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_STRING_LIST_DIRECTIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_STRING_LIST_DIRECTIVE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ContentSecurityPolicy;

class CORE_EXPORT StringListDirective final : public CSPDirective {
 public:
  StringListDirective(const String& name,
                      const String& value,
                      ContentSecurityPolicy*);
  void Trace(blink::Visitor*) override;
  bool Allows(const String& string_piece);

 private:
  // Determine whether a given string in the string list is valid.
  static bool IsInvalidStringValue(const String& str);

  Vector<String> list_;
  bool allow_any_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_STRING_LIST_DIRECTIVE_H_
