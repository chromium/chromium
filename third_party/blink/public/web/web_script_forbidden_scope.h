// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SCRIPT_FORBIDDEN_SCOPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SCRIPT_FORBIDDEN_SCOPE_H_

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class BLINK_EXPORT WebScriptForbiddenScope {
 public:
  WebScriptForbiddenScope() = delete;

  // Returns true if script execution is currently forbidden on the calling
  // thread.
  static bool IsForbidden();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SCRIPT_FORBIDDEN_SCOPE_H_
