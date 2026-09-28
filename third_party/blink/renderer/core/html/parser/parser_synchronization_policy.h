// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PARSER_SYNCHRONIZATION_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PARSER_SYNCHRONIZATION_POLICY_H_

namespace blink {

enum ParserSynchronizationPolicy {
  kAllowDeferredParsing,
  kForceSynchronousParsing,
};
}

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PARSER_SYNCHRONIZATION_POLICY_H_
