// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PARSER_SCRIPTING_FLAG_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PARSER_SCRIPTING_FLAG_POLICY_H_

namespace blink {

// Decides whether the scripting flag of the parser should be set to enabled.
// https://html.spec.whatwg.org/#scripting-flag
enum class ParserScriptingFlagPolicy { kOnlyIfScriptIsEnabled, kEnabled };

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PARSER_SCRIPTING_FLAG_POLICY_H_
