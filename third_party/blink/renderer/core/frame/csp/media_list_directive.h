// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_MEDIA_LIST_DIRECTIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_MEDIA_LIST_DIRECTIVE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ContentSecurityPolicy;

class CORE_EXPORT MediaListDirective final : public CSPDirective {
 public:
  MediaListDirective(const String& name,
                     const String& value,
                     ContentSecurityPolicy*);
  bool Allows(const String& type) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaListDirectiveTest, GetIntersect);
  FRIEND_TEST_ALL_PREFIXES(MediaListDirectiveTest, Subsumes);

  void Parse(const UChar* begin, const UChar* end);

  HashSet<String> plugin_types_;

  DISALLOW_COPY_AND_ASSIGN(MediaListDirective);
};

}  // namespace blink

#endif
