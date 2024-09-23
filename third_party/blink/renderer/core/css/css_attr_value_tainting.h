// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Values coming from attr() need to, per spec, be _tainted_. This taint
// is invisible, but using a tainted value inside a URL (whether it is
// url(), image-set(), or similar) is not legal and will result in an error.
// We implement this tainting by means of appending a special comment;
// it will then be kept throughout most processing but generally ignored
// by parsing (except for that, of course, URL-parsing functions must check
// for it using IsAttrTainted()). There are very few places such comments
// would be visible to authors, but we do have functionality to strip them
// out again for the cases where it matters.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ATTR_VALUE_TAINTING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ATTR_VALUE_TAINTING_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class CSSParserTokenStream;

CORE_EXPORT StringView GetCSSAttrTaintToken();

// NOTE: IsAttrTainted() means “is this value tainted by an attr()”,
// not “is this attr() value tainted” (it always is). In other words,
// it should be called on strings that you intend to parse as URLs,
// even if you are not parsing an attribute yourself.
bool IsAttrTainted(const CSSParserTokenStream& stream,
                   wtf_size_t start_offset,
                   wtf_size_t end_offset);
bool IsAttrTainted(StringView str);

String RemoveAttrTaintToken(StringView str);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ATTR_VALUE_TAINTING_H_
