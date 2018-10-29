/*
 * Copyright (C) 2011 Adam Barth. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_XSS_AUDITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_XSS_AUDITOR_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/text/suffix_tree.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

class Document;
class HTMLSourceTracker;
class XSSInfo;
class XSSAuditorDelegate;

struct FilterTokenRequest {
  STACK_ALLOCATED();

 public:
  FilterTokenRequest(HTMLToken& token,
                     HTMLSourceTracker& source_tracker,
                     bool should_allow_cdata)
      : token(token),
        source_tracker(source_tracker),
        should_allow_cdata(should_allow_cdata) {}

  HTMLToken& token;
  HTMLSourceTracker& source_tracker;
  bool should_allow_cdata;
};

class XSSAuditor {
  USING_FAST_MALLOC(XSSAuditor);

 public:
  XSSAuditor();

  void Init(Document*, XSSAuditorDelegate*);
  void InitForFragment();

  std::unique_ptr<XSSInfo> FilterToken(const FilterTokenRequest&);
  bool IsSafeToSendToAnotherThread() const;

  void SetEncoding(const WTF::TextEncoding&);

  bool IsEnabled() const { return is_enabled_; }

 private:
  static const wtf_size_t kMaximumFragmentLengthTarget = 100;

  enum State {
    kUninitialized,
    kFilteringTokens,
    kPermittingAdjacentCharacterTokens,
    kSuppressingAdjacentCharacterTokens
  };

  enum TruncationKind {
    kNoTruncation,
    kNormalAttributeTruncation,
    kSrcLikeAttributeTruncation,
    kScriptLikeAttributeTruncation,
    kSemicolonSeparatedScriptLikeAttributeTruncation,
  };

  enum HrefRestriction { kProhibitSameOriginHref, kAllowSameOriginHref };

  bool FilterStartToken(const FilterTokenRequest&);
  void FilterEndToken(const FilterTokenRequest&);
  bool FilterCharacterToken(const FilterTokenRequest&);
  bool FilterScriptToken(const FilterTokenRequest&);
  bool FilterObjectToken(const FilterTokenRequest&);
  bool FilterParamToken(const FilterTokenRequest&);
  bool FilterEmbedToken(const FilterTokenRequest&);
  bool FilterFrameToken(const FilterTokenRequest&);
  bool FilterMetaToken(const FilterTokenRequest&);
  bool FilterBaseToken(const FilterTokenRequest&);
  bool FilterFormToken(const FilterTokenRequest&);
  bool FilterInputToken(const FilterTokenRequest&);
  bool FilterButtonToken(const FilterTokenRequest&);
  bool FilterLinkToken(const FilterTokenRequest&);

  bool EraseDangerousAttributesIfInjected(const FilterTokenRequest&);
  bool EraseAttributeIfInjected(const FilterTokenRequest&,
                                const QualifiedName&,
                                const String& replacement_value = String(),
                                TruncationKind = kNormalAttributeTruncation,
                                HrefRestriction = kProhibitSameOriginHref);

  String CanonicalizedSnippetForTagName(const FilterTokenRequest&);
  String CanonicalizedSnippetForJavaScript(const FilterTokenRequest&);
  String NameFromAttribute(const FilterTokenRequest&,
                           const HTMLToken::Attribute&);
  String SnippetFromAttribute(const FilterTokenRequest&,
                              const HTMLToken::Attribute&);
  String Canonicalize(String, TruncationKind);

  bool IsContainedInRequest(const String&);
  bool IsLikelySafeResource(const String& url);

  KURL document_url_;
  bool is_enabled_;

  ReflectedXSSDisposition xss_protection_;
  bool did_send_valid_xss_protection_header_;

  String decoded_url_;
  String decoded_http_body_;
  String http_body_as_string_;
  std::unique_ptr<SuffixTree<ASCIICodebook>> decoded_http_body_suffix_tree_;

  State state_;
  bool script_tag_found_in_request_;
  unsigned script_tag_nesting_level_;
  WTF::TextEncoding encoding_;

  DISALLOW_COPY_AND_ASSIGN(XSSAuditor);
};

}  // namespace blink

#endif
