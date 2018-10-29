/*
 * Copyright (C) 2008, 2010 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/css_preload_scanner.h"

#include <memory>

#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/text/segmented_string.h"

namespace blink {

CSSPreloadScanner::CSSPreloadScanner() = default;

CSSPreloadScanner::~CSSPreloadScanner() = default;

void CSSPreloadScanner::Reset() {
  state_ = kInitial;
  rule_.Clear();
  rule_value_.Clear();
}

template <typename Char>
void CSSPreloadScanner::ScanCommon(const Char* begin,
                                   const Char* end,
                                   const SegmentedString& source,
                                   PreloadRequestStream& requests,
                                   const KURL& predicted_base_element_url) {
  requests_ = &requests;
  predicted_base_element_url_ = &predicted_base_element_url;

  for (const Char* it = begin; it != end && state_ != kDoneParsingImportRules;
       ++it)
    Tokenize(*it, source);

  requests_ = nullptr;
  predicted_base_element_url_ = nullptr;
}

void CSSPreloadScanner::Scan(const HTMLToken::DataVector& data,
                             const SegmentedString& source,
                             PreloadRequestStream& requests,
                             const KURL& predicted_base_element_url) {
  ScanCommon(data.data(), data.data() + data.size(), source, requests,
             predicted_base_element_url);
}

void CSSPreloadScanner::Scan(const String& tag_name,
                             const SegmentedString& source,
                             PreloadRequestStream& requests,
                             const KURL& predicted_base_element_url) {
  if (tag_name.Is8Bit()) {
    const LChar* begin = tag_name.Characters8();
    ScanCommon(begin, begin + tag_name.length(), source, requests,
               predicted_base_element_url);
    return;
  }
  const UChar* begin = tag_name.Characters16();
  ScanCommon(begin, begin + tag_name.length(), source, requests,
             predicted_base_element_url);
}

void CSSPreloadScanner::SetReferrerPolicy(const ReferrerPolicy policy) {
  referrer_policy_ = policy;
}

inline void CSSPreloadScanner::Tokenize(UChar c,
                                        const SegmentedString& source) {
  // We are just interested in @import rules, no need for real tokenization here
  // Searching for other types of resources is probably low payoff.
  // If we ever decide to preload fonts, we also need to change
  // ResourceFetcher::resourceNeedsLoad to immediately load speculative font
  // preloads.
  switch (state_) {
    case kInitial:
      if (IsHTMLSpace<UChar>(c))
        break;
      if (c == '@')
        state_ = kRuleStart;
      else if (c == '/')
        state_ = kMaybeComment;
      else
        state_ = kDoneParsingImportRules;
      break;
    case kMaybeComment:
      if (c == '*')
        state_ = kComment;
      else
        state_ = kInitial;
      break;
    case kComment:
      if (c == '*')
        state_ = kMaybeCommentEnd;
      break;
    case kMaybeCommentEnd:
      if (c == '*')
        break;
      if (c == '/')
        state_ = kInitial;
      else
        state_ = kComment;
      break;
    case kRuleStart:
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
        rule_.Clear();
        rule_value_.Clear();
        rule_.Append(c);
        state_ = kRule;
      } else
        state_ = kInitial;
      break;
    case kRule:
      if (IsHTMLSpace<UChar>(c))
        state_ = kAfterRule;
      else if (c == ';')
        state_ = kInitial;
      else
        rule_.Append(c);
      break;
    case kAfterRule:
      if (IsHTMLSpace<UChar>(c))
        break;
      if (c == ';')
        state_ = kInitial;
      else if (c == '{')
        state_ = kDoneParsingImportRules;
      else {
        state_ = kRuleValue;
        rule_value_.Append(c);
      }
      break;
    case kRuleValue:
      if (IsHTMLSpace<UChar>(c))
        state_ = kAfterRuleValue;
      else if (c == ';')
        EmitRule(source);
      else
        rule_value_.Append(c);
      break;
    case kAfterRuleValue:
      if (IsHTMLSpace<UChar>(c))
        break;
      if (c == ';')
        EmitRule(source);
      else if (c == '{')
        state_ = kDoneParsingImportRules;
      else {
        // FIXME: media rules
        state_ = kInitial;
      }
      break;
    case kDoneParsingImportRules:
      NOTREACHED();
      break;
  }
}

static String ParseCSSStringOrURL(const String& string) {
  wtf_size_t offset = 0;
  wtf_size_t reduced_length = string.length();

  while (reduced_length && IsHTMLSpace<UChar>(string[offset])) {
    ++offset;
    --reduced_length;
  }
  while (reduced_length &&
         IsHTMLSpace<UChar>(string[offset + reduced_length - 1]))
    --reduced_length;

  if (reduced_length >= 5 && (string[offset] == 'u' || string[offset] == 'U') &&
      (string[offset + 1] == 'r' || string[offset + 1] == 'R') &&
      (string[offset + 2] == 'l' || string[offset + 2] == 'L') &&
      string[offset + 3] == '(' && string[offset + reduced_length - 1] == ')') {
    offset += 4;
    reduced_length -= 5;
  }

  while (reduced_length && IsHTMLSpace<UChar>(string[offset])) {
    ++offset;
    --reduced_length;
  }
  while (reduced_length &&
         IsHTMLSpace<UChar>(string[offset + reduced_length - 1]))
    --reduced_length;

  if (reduced_length < 2 ||
      string[offset] != string[offset + reduced_length - 1] ||
      !(string[offset] == '\'' || string[offset] == '"'))
    return String();
  offset++;
  reduced_length -= 2;

  while (reduced_length && IsHTMLSpace<UChar>(string[offset])) {
    ++offset;
    --reduced_length;
  }
  while (reduced_length &&
         IsHTMLSpace<UChar>(string[offset + reduced_length - 1]))
    --reduced_length;

  return string.Substring(offset, reduced_length);
}

void CSSPreloadScanner::EmitRule(const SegmentedString& source) {
  if (DeprecatedEqualIgnoringCase(rule_, "import")) {
    String url = ParseCSSStringOrURL(rule_value_.ToString());
    TextPosition position =
        TextPosition(source.CurrentLine(), source.CurrentColumn());
    auto request = PreloadRequest::CreateIfNeeded(
        FetchInitiatorTypeNames::css, position, url,
        *predicted_base_element_url_, ResourceType::kCSSStyleSheet,
        referrer_policy_, PreloadRequest::kBaseUrlIsReferrer,
        ResourceFetcher::kImageNotImageSet);
    if (request) {
      // FIXME: Should this be including the charset in the preload request?
      requests_->push_back(std::move(request));
    }
    state_ = kInitial;
  } else if (DeprecatedEqualIgnoringCase(rule_, "charset"))
    state_ = kInitial;
  else
    state_ = kDoneParsingImportRules;
  rule_.Clear();
  rule_value_.Clear();
}

}  // namespace blink
