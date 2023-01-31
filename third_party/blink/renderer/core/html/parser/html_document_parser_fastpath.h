// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_DOCUMENT_PARSER_FASTPATH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_DOCUMENT_PARSER_FASTPATH_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/parser_content_policy.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class Document;
class DocumentFragment;
class Element;

CORE_EXPORT bool TryParsingHTMLFragment(const String& source,
                                        Document& document,
                                        DocumentFragment& fragment,
                                        Element& context_element,
                                        ParserContentPolicy policy,
                                        bool include_shadow_roots);

// Captures the potential outcomes for fast path html parser.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This is exposed for testing.
enum class HtmlFastPathResult {
  kSucceeded = 0,
  kFailedTracingEnabled = 1,
  kFailedParserContentPolicy = 2,
  kFailedInForm = 3,
  kFailedUnsupportedContextTag = 4,
  kFailedOptionWithChild = 5,
  kFailedDidntReachEndOfInput = 6,
  kFailedContainsNull = 7,
  kFailedParsingTagName = 8,
  kFailedParsingQuotedAttributeValue = 9,
  kFailedParsingUnquotedAttributeValue = 10,
  kFailedParsingQuotedEscapedAttributeValue = 11,
  kFailedParsingUnquotedEscapedAttributeValue = 12,
  kFailedParsingCharacterReference = 13,
  kFailedEndOfInputReached = 14,
  kFailedParsingAttributes = 15,
  kFailedParsingSpecificElements = 16,
  kFailedParsingElement = 17,
  kFailedUnsupportedTag = 18,
  kFailedEndOfInputReachedForContainer = 19,
  kFailedUnexpectedTagNameCloseState = 20,
  kFailedEndTagNameMismatch = 21,
  kFailedShadowRoots = 22,
  // This value is no longer used.
  // kFailedDirAttributeDirty = 23,
  kFailedOnAttribute = 24,
  kFailedMaxDepth = 25,
  kFailedBigText = 25,
  kFailedCssPseudoDirEnabledAndDirAttributeDirty = 26,
  kMaxValue = kFailedCssPseudoDirEnabledAndDirAttributeDirty,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_DOCUMENT_PARSER_FASTPATH_H_
