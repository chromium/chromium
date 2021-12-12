// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CLIENT_HINTS_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CLIENT_HINTS_UTIL_H_

#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class KURL;
class LocalDOMWindow;

// This primarily calls ClientHintsPreferences::UpdateFromMetaTagAcceptCH,
// but then updates the `execution_context` with any relevant permissions
// policy changes for client hints due to the named meta tag. Ideally, this
// would be a part of ClientHintsPreferences, but we cannot access files in
// third_party/blink/renderer/core from third_party/blink/renderer/platform.
// TODO(crbug.com/1278127): Replace w/ generic HTML policy modification.
void UpdateWindowPermissionsPolicyWithDelegationSupportForClientHints(
    ClientHintsPreferences& client_hints_preferences,
    LocalDOMWindow* local_dom_window,
    const String& header_value,
    const KURL& url,
    ClientHintsPreferences::Context* context,
    bool is_http_equiv,
    bool is_preload_or_sync_parser);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CLIENT_HINTS_UTIL_H_
