/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_CSS_PRELOAD_SCANNER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_CSS_PRELOAD_SCANNER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html/parser/preload_request.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class SegmentedString;

class CSSPreloadScanner {
  DISALLOW_NEW();

 public:
  CSSPreloadScanner();
  ~CSSPreloadScanner();

  void Reset();

  void Scan(const HTMLToken::DataVector&,
            const SegmentedString&,
            PreloadRequestStream&,
            const KURL&);
  void Scan(const String&,
            const SegmentedString&,
            PreloadRequestStream&,
            const KURL&);

  void SetReferrerPolicy(network::mojom::ReferrerPolicy);

 private:
  enum State {
    kInitial,
    kMaybeComment,
    kComment,
    kMaybeCommentEnd,
    kRuleStart,
    kRule,
    kAfterRule,
    kRuleValue,
    kAfterRuleValue,
    kDoneParsingImportRules,
  };

  template <typename Char>
  void ScanCommon(const Char* begin,
                  const Char* end,
                  const SegmentedString&,
                  PreloadRequestStream&,
                  const KURL&);

  inline void Tokenize(UChar, const SegmentedString&);
  void EmitRule(const SegmentedString&);

  bool HasFinishedRuleValue() const;

  State state_ = kInitial;
  StringBuilder rule_;
  StringBuilder rule_value_;

  network::mojom::ReferrerPolicy referrer_policy_ =
      network::mojom::ReferrerPolicy::kDefault;

  // Below members only non-null during scan()
  PreloadRequestStream* requests_ = nullptr;
  const KURL* predicted_base_element_url_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CSSPreloadScanner);
};

}  // namespace blink

#endif
