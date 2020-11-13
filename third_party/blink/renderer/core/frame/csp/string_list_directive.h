// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_STRING_LIST_DIRECTIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_STRING_LIST_DIRECTIVE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ContentSecurityPolicy;

class CORE_EXPORT StringListDirective final : public CSPDirective {
 public:
  StringListDirective(const String& name,
                      const String& value,
                      ContentSecurityPolicy*);
  void Trace(Visitor*) const override;
  bool Allows(
      const String& string_piece,
      bool is_duplicate,
      ContentSecurityPolicy::AllowTrustedTypePolicyDetails& violation_details);
  bool IsAllowDuplicates() const { return allow_duplicates_; }

 private:
  // Determine whether a given string is a valid policy name or a special token
  // ("*" or "'allow-duplicates'"). In the token case, set the appropriate flags
  // as a side effect.
  bool AllowOrProcessValue(const String& src);

  static bool IsPolicyName(const String& name);
  static bool IsNotPolicyNameChar(UChar c);

  Vector<String> list_;
  bool allow_any_;
  bool allow_duplicates_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_STRING_LIST_DIRECTIVE_H_
