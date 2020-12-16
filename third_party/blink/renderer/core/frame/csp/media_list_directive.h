// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_MEDIA_LIST_DIRECTIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_MEDIA_LIST_DIRECTIVE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ContentSecurityPolicy;

CORE_EXPORT
Vector<String> CSPPluginTypesParse(const String& value,
                                   ContentSecurityPolicy* policy);

}  // namespace blink

#endif
