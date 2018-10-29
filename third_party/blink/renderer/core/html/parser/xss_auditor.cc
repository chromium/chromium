/*
 * Copyright (C) 2011 Adam Barth. All Rights Reserved.
 * Copyright (C) 2011 Daniel Bates (dbates@intudata.com).
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

#include "third_party/blink/renderer/core/html/parser/xss_auditor.h"

#include <memory>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_param_element.h"
#include "third_party/blink/renderer/core/html/link_rel_attribute.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html/parser/xss_auditor_delegate.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/text/decode_escape_sequences.h"
#include "third_party/blink/renderer/platform/wtf/ascii_ctype.h"

namespace {

// SecurityOrigin::urlWithUniqueSecurityOrigin() can't be used cross-thread, or
// we'd use it instead.
const char kURLWithUniqueOrigin[] = "data:,";

const char kSafeJavaScriptURL[] = "javascript:void(0)";

}  // namespace

namespace blink {

using namespace HTMLNames;

static bool IsNonCanonicalCharacter(UChar c) {
  // We remove all non-ASCII characters, including non-printable ASCII
  // characters.
  //
  // Note, we don't remove backslashes like PHP stripslashes(), which among
  // other things converts "\\0" to the \0 character. Instead, we remove
  // backslashes and zeros (since the string "\\0" =(remove backslashes)=> "0").
  // However, this has the adverse effect that we remove any legitimate zeros
  // from a string.
  //
  // We also remove forward-slash, because it is common for some servers to
  // collapse successive path components, eg, a//b becomes a/b.
  //
  // We also remove the questionmark character, since some severs replace
  // invalid high-bytes with a questionmark. We are already stripping the
  // high-bytes so we also strip the questionmark to match.
  //
  // We also move the percent character, since some servers strip it when
  // there's a malformed sequence.
  //
  // For instance: new String("http://localhost:8000?x") => new
  // String("http:localhost:8x").
  return (c == '\\' || c == '0' || c == '\0' || c == '/' || c == '?' ||
          c == '%' || c >= 127);
}

static bool IsRequiredForInjection(UChar c) {
  return (c == '\'' || c == '"' || c == '<' || c == '>');
}

static bool IsTerminatingCharacter(UChar c) {
  return (c == '&' || c == '/' || c == '"' || c == '\'' || c == '<' ||
          c == '>' || c == ',' || c == ';');
}

static bool IsSlash(UChar c) {
  return (c == '/' || c == '\\');
}

static bool IsHTMLQuote(UChar c) {
  return (c == '"' || c == '\'');
}

static bool IsJSNewline(UChar c) {
  // Per ecma-262 section 7.3 Line Terminators.
  return (c == '\n' || c == '\r' || c == 0x2028 || c == 0x2029);
}

static bool StartsHTMLOpenCommentAt(const String& string, wtf_size_t start) {
  return (start + 3 < string.length() && string[start] == '<' &&
          string[start + 1] == '!' && string[start + 2] == '-' &&
          string[start + 3] == '-');
}

static bool StartsHTMLCloseCommentAt(const String& string, wtf_size_t start) {
  return (start + 2 < string.length() && string[start] == '-' &&
          string[start + 1] == '-' && string[start + 2] == '>');
}

static bool StartsSingleLineCommentAt(const String& string, wtf_size_t start) {
  return (start + 1 < string.length() && string[start] == '/' &&
          string[start + 1] == '/');
}

static bool StartsMultiLineCommentAt(const String& string, wtf_size_t start) {
  return (start + 1 < string.length() && string[start] == '/' &&
          string[start + 1] == '*');
}

static bool StartsOpeningScriptTagAt(const String& string, wtf_size_t start) {
  if (start + 6 >= string.length())
    return false;
  // TODO(esprehn): StringView should probably have startsWith.
  StringView script("<script");
  return EqualIgnoringASCIICase(StringView(string, start, script.length()),
                                script);
}

static bool StartsClosingScriptTagAt(const String& string, wtf_size_t start) {
  if (start + 7 >= string.length())
    return false;
  // TODO(esprehn): StringView should probably have startsWith.
  StringView script("</script");
  return EqualIgnoringASCIICase(StringView(string, start, script.length()),
                                script);
}

// If other files need this, we should move this to
// core/html/parser/html_parser_idioms.h
template <wtf_size_t inlineCapacity>
bool ThreadSafeMatch(const Vector<UChar, inlineCapacity>& vector,
                     const QualifiedName& qname) {
  return EqualIgnoringNullity(vector, qname.LocalName().Impl());
}

static bool HasName(const HTMLToken& token, const QualifiedName& name) {
  return ThreadSafeMatch(token.GetName(), name);
}

static bool FindAttributeWithName(const HTMLToken& token,
                                  const QualifiedName& name,
                                  wtf_size_t& index_of_matching_attribute) {
  // Notice that we're careful not to ref the StringImpl here because we might
  // be on a background thread.
  const String& attr_name = name.NamespaceURI() == xlink_names::kNamespaceURI
                                ? "xlink:" + name.LocalName().GetString()
                                : name.LocalName().GetString();

  for (wtf_size_t i = 0; i < token.Attributes().size(); ++i) {
    if (EqualIgnoringNullity(token.Attributes().at(i).NameAsVector(),
                             attr_name)) {
      index_of_matching_attribute = i;
      return true;
    }
  }
  return false;
}

static bool IsNameOfInlineEventHandler(const Vector<UChar, 32>& name) {
  const wtf_size_t kLengthOfShortestInlineEventHandlerName =
      5;  // To wit: oncut.
  if (name.size() < kLengthOfShortestInlineEventHandlerName)
    return false;
  return name[0] == 'o' && name[1] == 'n';
}

static bool IsDangerousHTTPEquiv(const String& value) {
  String equiv = value.StripWhiteSpace();
  return DeprecatedEqualIgnoringCase(equiv, "refresh") ||
         DeprecatedEqualIgnoringCase(equiv, "set-cookie");
}

static inline String Decode16BitUnicodeEscapeSequences(const String& string) {
  // Note, the encoding is ignored since each %u-escape sequence represents a
  // UTF-16 code unit.
  return DecodeEscapeSequences<Unicode16BitEscapeSequence>(string,
                                                           UTF8Encoding());
}

static inline String DecodeStandardURLEscapeSequences(
    const String& string,
    const WTF::TextEncoding& encoding) {
  // We use DecodeEscapeSequences() instead of DecodeURLEscapeSequences()
  // (declared in weborigin/kurl.h) to avoid platform-specific URL decoding
  // differences (e.g. KURLGoogle).
  return DecodeEscapeSequences<URLEscapeSequence>(string, encoding);
}

static String FullyDecodeString(const String& string,
                                const WTF::TextEncoding& encoding) {
  wtf_size_t old_working_string_length;
  String working_string = string;
  do {
    old_working_string_length = working_string.length();
    working_string = Decode16BitUnicodeEscapeSequences(
        DecodeStandardURLEscapeSequences(working_string, encoding));
  } while (working_string.length() < old_working_string_length);
  working_string.Replace('+', ' ');
  return working_string;
}

// XSSAuditor's task is to determine how much of any given content came
// from a reflection vs. what occurs normally on the page. It must do
// this in face of an attacker avoiding detection by splicing on page
// content in such a way as to remain syntactically valid. The next two
// functions apply heurisitcs to get the longest possible fragment in
// face of such trickery.

static void TruncateForSrcLikeAttribute(String& decoded_snippet) {
  // In HTTP URLs, characters in the query string (following the first ?),
  // in the fragment (following the first #), or even in the path (typically
  // following the third slash but subject to generous interpretation of a
  // lack of leading slashes) may be merely ignored by an attacker's server
  // when a remote script or script-like resource is requested. Hence these
  // are places where organic page content may be spliced.
  //
  // In DATA URLS, the payload starts at the first comma, and the the first
  //  "/*", "//", or "<!--" may introduce a comment, which can then be used
  // to splice page data harmlessly onto the end of the payload.
  //
  // Also, DATA URLs may use the same string literal tricks as with script
  // content itself. In either case, content following this may come from the
  // page and may be ignored when the script is executed. Also, any of these
  // characters may now be represented by the (enlarged) set of html5 entities.
  //
  // For simplicity, we don't differentiate based on URL scheme, and stop at
  // any of the following:
  //   - the first &, since it might be part of an entity for any of the
  //     subsequent punctuation.
  //   - the first # or ?, since the query and fragment can be ignored.
  //   - the third slash, since this typically starts the path, but account
  //     for a possible lack of leading slashes following the scheme).
  //   - the first slash, <, ', or " once a comma is seen, since we
  //     may now be in a data URL payload.
  int slash_count = 0;
  bool comma_seen = false;
  bool colon_seen = false;
  for (wtf_size_t current_length = 0,
                  remaining_length = decoded_snippet.length();
       remaining_length; ++current_length, --remaining_length) {
    UChar current_char = decoded_snippet[current_length];
    if (current_char == ':' && !colon_seen) {
      if (remaining_length > 1 && !IsSlash(decoded_snippet[current_length + 1]))
        ++slash_count;
      if (remaining_length > 2 && !IsSlash(decoded_snippet[current_length + 2]))
        ++slash_count;
      colon_seen = true;
    }
    if (current_char == '&' || current_char == '?' || current_char == '#' ||
        (IsSlash(current_char) && (comma_seen || ++slash_count > 2)) ||
        (current_char == '<' && comma_seen) ||
        (current_char == '\'' && comma_seen) ||
        (current_char == '"' && comma_seen)) {
      decoded_snippet.Truncate(current_length);
      return;
    }
    if (current_char == ',')
      comma_seen = true;
  }
}

static void TruncateForScriptLikeAttribute(String& decoded_snippet) {
  // Beware of trailing characters which came from the page itself, not the
  // injected vector. Excluding the terminating character covers common cases
  // where the page immediately ends the attribute, but doesn't cover more
  // complex cases where there is other page data following the injection.
  //
  // Generally, these won't parse as javascript, so the injected vector
  // typically excludes them from consideration via a single-line comment or
  // by enclosing them in a string literal terminated later by the page's own
  // closing punctuation. Since the snippet has not been parsed, the vector
  // may also try to introduce these via entities. As a result, we'd like to
  // stop before the first "//", the first <!--, the first entity, or the first
  // quote not immediately following the first equals sign (taking whitespace
  // into consideration).
  //
  // To keep things simpler, we don't try to distinguish between
  // entity-introducing amperands vs. other uses, nor do we bother to check for
  // a second slash for a comment, nor do we bother to check for !-- following a
  // less-than sign. We stop instead on any ampersand slash, or less-than sign.
  wtf_size_t position = 0;
  if ((position = decoded_snippet.Find("=")) != kNotFound &&
      (position = decoded_snippet.Find(IsNotHTMLSpace<UChar>, position + 1)) !=
          kNotFound &&
      (position = decoded_snippet.Find(
           IsTerminatingCharacter,
           IsHTMLQuote(decoded_snippet[position]) ? position + 1 : position)) !=
          kNotFound) {
    decoded_snippet.Truncate(position);
  }
}

static void TruncateForSemicolonSeparatedScriptLikeAttribute(
    String& decoded_snippet) {
  // Same as script-like attributes, but semicolons can introduce page data.
  TruncateForScriptLikeAttribute(decoded_snippet);
  wtf_size_t position = decoded_snippet.Find(";");
  if (position != kNotFound)
    decoded_snippet.Truncate(position);
}

static bool IsSemicolonSeparatedAttribute(
    const HTMLToken::Attribute& attribute) {
  return ThreadSafeMatch(attribute.NameAsVector(), svg_names::kValuesAttr);
}

static bool IsSemicolonSeparatedValueContainingJavaScriptURL(
    const String& value) {
  Vector<String> value_list;
  value.Split(';', value_list);
  for (wtf_size_t i = 0; i < value_list.size(); ++i) {
    String stripped = StripLeadingAndTrailingHTMLSpaces(value_list[i]);
    if (ProtocolIsJavaScript(stripped))
      return true;
  }
  return false;
}

XSSAuditor::XSSAuditor()
    : is_enabled_(false),
      xss_protection_(kFilterReflectedXSS),
      did_send_valid_xss_protection_header_(false),
      state_(kUninitialized),
      script_tag_found_in_request_(false),
      script_tag_nesting_level_(0),
      encoding_(UTF8Encoding()) {
  // Although tempting to call init() at this point, the various objects
  // we want to reference might not all have been constructed yet.
}

void XSSAuditor::InitForFragment() {
  DCHECK(IsMainThread());
  DCHECK_EQ(state_, kUninitialized);
  state_ = kFilteringTokens;
  // When parsing a fragment, we don't enable the XSS auditor because it's
  // too much overhead.
  DCHECK(!is_enabled_);
}

void XSSAuditor::Init(Document* document,
                      XSSAuditorDelegate* auditor_delegate) {
  DCHECK(IsMainThread());
  if (state_ != kUninitialized)
    return;
  state_ = kFilteringTokens;

  if (Settings* settings = document->GetSettings())
    is_enabled_ = settings->GetXSSAuditorEnabled();

  if (!is_enabled_)
    return;

  document_url_ = document->Url().Copy();

  // In theory, the Document could have detached from the LocalFrame after the
  // XSSAuditor was constructed.
  if (!document->GetFrame()) {
    is_enabled_ = false;
    return;
  }

  if (document_url_.IsEmpty()) {
    // The URL can be empty when opening a new browser window or calling
    // window.open("").
    is_enabled_ = false;
    return;
  }

  if (document_url_.ProtocolIsData()) {
    is_enabled_ = false;
    return;
  }

  if (document->Encoding().IsValid())
    encoding_ = document->Encoding();

  if (DocumentLoader* document_loader =
          document->GetFrame()->Loader().GetDocumentLoader()) {
    const AtomicString& header_value =
        document_loader->GetResponse().HttpHeaderField(
            HTTPNames::X_XSS_Protection);
    String error_details;
    unsigned error_position = 0;
    String report_url;
    KURL xss_protection_report_url;

    ReflectedXSSDisposition xss_protection_header = ParseXSSProtectionHeader(
        header_value, error_details, error_position, report_url);

    if (xss_protection_header == kAllowReflectedXSS)
      UseCounter::Count(*document, WebFeature::kXSSAuditorDisabled);
    else if (xss_protection_header == kFilterReflectedXSS)
      UseCounter::Count(*document, WebFeature::kXSSAuditorEnabledFilter);
    else if (xss_protection_header == kBlockReflectedXSS)
      UseCounter::Count(*document, WebFeature::kXSSAuditorEnabledBlock);
    else if (xss_protection_header == kReflectedXSSInvalid)
      UseCounter::Count(*document, WebFeature::kXSSAuditorInvalid);

    did_send_valid_xss_protection_header_ =
        xss_protection_header != kReflectedXSSUnset &&
        xss_protection_header != kReflectedXSSInvalid;
    if ((xss_protection_header == kFilterReflectedXSS ||
         xss_protection_header == kBlockReflectedXSS) &&
        !report_url.IsEmpty()) {
      xss_protection_report_url = document->CompleteURL(report_url);
      if (MixedContentChecker::IsMixedContent(document->GetSecurityOrigin(),
                                              xss_protection_report_url)) {
        error_details = "insecure reporting URL for secure page";
        xss_protection_header = kReflectedXSSInvalid;
        xss_protection_report_url = KURL();
      }
    }
    if (xss_protection_header == kReflectedXSSInvalid) {
      document->AddConsoleMessage(ConsoleMessage::Create(
          kSecurityMessageSource, kErrorMessageLevel,
          "Error parsing header X-XSS-Protection: " + header_value + ": " +
              error_details + " at character position " +
              String::Format("%u", error_position) +
              ". The default protections will be applied."));
    }

    xss_protection_ = xss_protection_header;
    if (xss_protection_ == kReflectedXSSInvalid ||
        xss_protection_ == kReflectedXSSUnset) {
      xss_protection_ = kBlockReflectedXSS;
    }

    if (auditor_delegate)
      auditor_delegate->SetReportURL(xss_protection_report_url.Copy());

    EncodedFormData* http_body = document_loader->GetRequest().HttpBody();
    if (http_body && !http_body->IsEmpty())
      http_body_as_string_ = http_body->FlattenToString();
  }

  SetEncoding(encoding_);
}

void XSSAuditor::SetEncoding(const WTF::TextEncoding& encoding) {
  const wtf_size_t kMiniumLengthForSuffixTree =
      512;  // FIXME: Tune this parameter.
  const int kSuffixTreeDepth = 5;

  if (!encoding.IsValid())
    return;

  encoding_ = encoding;

  decoded_url_ = Canonicalize(document_url_.GetString(), kNoTruncation);
  if (decoded_url_.Find(IsRequiredForInjection) == kNotFound)
    decoded_url_ = String();

  if (!http_body_as_string_.IsEmpty()) {
    decoded_http_body_ = Canonicalize(http_body_as_string_, kNoTruncation);
    http_body_as_string_ = String();
    if (decoded_http_body_.Find(IsRequiredForInjection) == kNotFound)
      decoded_http_body_ = String();
    if (decoded_http_body_.length() >= kMiniumLengthForSuffixTree) {
      decoded_http_body_suffix_tree_ =
          std::make_unique<SuffixTree<ASCIICodebook>>(decoded_http_body_,
                                                      kSuffixTreeDepth);
    }
  }

  if (decoded_url_.IsEmpty() && decoded_http_body_.IsEmpty())
    is_enabled_ = false;
}

std::unique_ptr<XSSInfo> XSSAuditor::FilterToken(
    const FilterTokenRequest& request) {
  DCHECK_NE(state_, kUninitialized);
  if (!is_enabled_ || xss_protection_ == kAllowReflectedXSS)
    return nullptr;

  bool did_block_script = false;
  if (request.token.GetType() == HTMLToken::kStartTag)
    did_block_script = FilterStartToken(request);
  else if (script_tag_nesting_level_) {
    if (request.token.GetType() == HTMLToken::kCharacter)
      did_block_script = FilterCharacterToken(request);
    else if (request.token.GetType() == HTMLToken::kEndTag)
      FilterEndToken(request);
  }

  if (did_block_script) {
    bool did_block_entire_page = (xss_protection_ == kBlockReflectedXSS);
    std::unique_ptr<XSSInfo> xss_info =
        XSSInfo::Create(document_url_, did_block_entire_page,
                        did_send_valid_xss_protection_header_);
    return xss_info;
  }
  return nullptr;
}

bool XSSAuditor::FilterStartToken(const FilterTokenRequest& request) {
  state_ = kFilteringTokens;
  bool did_block_script = EraseDangerousAttributesIfInjected(request);

  if (HasName(request.token, scriptTag)) {
    did_block_script |= FilterScriptToken(request);
    DCHECK(request.should_allow_cdata || !script_tag_nesting_level_);
    script_tag_nesting_level_++;
  } else if (HasName(request.token, objectTag))
    did_block_script |= FilterObjectToken(request);
  else if (HasName(request.token, paramTag))
    did_block_script |= FilterParamToken(request);
  else if (HasName(request.token, embedTag))
    did_block_script |= FilterEmbedToken(request);
  else if (HasName(request.token, iframeTag) ||
           HasName(request.token, frameTag))
    did_block_script |= FilterFrameToken(request);
  else if (HasName(request.token, metaTag))
    did_block_script |= FilterMetaToken(request);
  else if (HasName(request.token, baseTag))
    did_block_script |= FilterBaseToken(request);
  else if (HasName(request.token, formTag))
    did_block_script |= FilterFormToken(request);
  else if (HasName(request.token, inputTag))
    did_block_script |= FilterInputToken(request);
  else if (HasName(request.token, buttonTag))
    did_block_script |= FilterButtonToken(request);
  else if (HasName(request.token, linkTag))
    did_block_script |= FilterLinkToken(request);

  return did_block_script;
}

void XSSAuditor::FilterEndToken(const FilterTokenRequest& request) {
  DCHECK(script_tag_nesting_level_);
  state_ = kFilteringTokens;
  if (HasName(request.token, scriptTag)) {
    script_tag_nesting_level_--;
    DCHECK(request.should_allow_cdata || !script_tag_nesting_level_);
  }
}

bool XSSAuditor::FilterCharacterToken(const FilterTokenRequest& request) {
  DCHECK(script_tag_nesting_level_);
  DCHECK_NE(state_, kUninitialized);
  if (state_ == kPermittingAdjacentCharacterTokens)
    return false;

  if (state_ == kFilteringTokens && script_tag_found_in_request_) {
    String snippet = CanonicalizedSnippetForJavaScript(request);
    if (IsContainedInRequest(snippet))
      state_ = kSuppressingAdjacentCharacterTokens;
    else if (!snippet.IsEmpty())
      state_ = kPermittingAdjacentCharacterTokens;
  }
  if (state_ == kSuppressingAdjacentCharacterTokens) {
    request.token.EraseCharacters();
    // Technically, character tokens can't be empty.
    request.token.AppendToCharacter(' ');
    return true;
  }
  return false;
}

bool XSSAuditor::FilterScriptToken(const FilterTokenRequest& request) {
  DCHECK_EQ(request.token.GetType(), HTMLToken::kStartTag);
  DCHECK(HasName(request.token, scriptTag));

  bool did_block_script = false;
  script_tag_found_in_request_ =
      IsContainedInRequest(CanonicalizedSnippetForTagName(request));
  if (script_tag_found_in_request_) {
    did_block_script |= EraseAttributeIfInjected(
        request, srcAttr, BlankURL().GetString(), kSrcLikeAttributeTruncation);
    did_block_script |= EraseAttributeIfInjected(request, svg_names::kHrefAttr,
                                                 BlankURL().GetString(),
                                                 kSrcLikeAttributeTruncation);
    did_block_script |= EraseAttributeIfInjected(
        request, xlink_names::kHrefAttr, BlankURL().GetString(),
        kSrcLikeAttributeTruncation);
  }
  return did_block_script;
}

bool XSSAuditor::FilterObjectToken(const FilterTokenRequest& request) {
  DCHECK_EQ(request.token.GetType(), HTMLToken::kStartTag);
  DCHECK(HasName(request.token, objectTag));

  bool did_block_script = false;
  if (IsContainedInRequest(CanonicalizedSnippetForTagName(request))) {
    did_block_script |= EraseAttributeIfInjected(
        request, dataAttr, BlankURL().GetString(), kSrcLikeAttributeTruncation);
    did_block_script |= EraseAttributeIfInjected(request, typeAttr);
    did_block_script |= EraseAttributeIfInjected(request, classidAttr);
  }
  return did_block_script;
}

bool XSSAuditor::FilterParamToken(const FilterTokenRequest& request) {
  DCHECK_EQ(request.token.GetType(), HTMLToken::kStartTag);
  DCHECK(HasName(request.token, paramTag));

  wtf_size_t index_of_name_attribute;
  if (!FindAttributeWithName(request.token, nameAttr, index_of_name_attribute))
    return false;

  const HTMLToken::Attribute& name_attribute =
      request.token.Attributes().at(index_of_name_attribute);
  if (!HTMLParamElement::IsURLParameter(name_attribute.Value()))
    return false;

  return EraseAttributeIfInjected(request, valueAttr, BlankURL().GetString(),
                                  kSrcLikeAttributeTruncation);
}

bool XSSAuditor::FilterEmbedToken(const FilterTokenRequest& request) {
  DCHECK_EQ(request.token.GetType(), HTMLToken::kStartTag);
  DCHECK(HasName(request.token, embedTag));

  bool did_block_script = false;
  if (IsContainedInRequest(CanonicalizedSnippetForTagName(request))) {
    did_block_script |= EraseAttributeIfInjected(request, codeAttr, String(),
                                                 kSrcLikeAttributeTruncation);
    did_block_script |= EraseAttributeIfInjected(
        request, srcAttr, BlankURL().GetString(), kSrcLikeAttributeTruncation);
    did_block_script |= EraseAttributeIfInjected(request, typeAttr);
  }
  return did_block_script;
}

bool XSSAuditor::FilterFrameToken(const FilterTokenRequest& request) {
  DCHECK_EQ(request.token.GetType(), HTMLToken::kStartTag);
  DCHECK(HasName(request.token, iframeTag) || HasName(request.token, frameTag));

  bool did_block_script = EraseAttributeIfInjected(
      request, srcdocAttr, String(), kScriptLikeAttributeTruncation);
  if (IsContainedInRequest(CanonicalizedSnippetForTagName(request)))
    did_block_script |= EraseAttributeIfInjected(request, srcAttr, String(),
                                                 kSrcLikeAttributeTruncation);

  return did_block_script;
}

bool XSSAuditor::FilterMetaToken(const FilterTokenRequest& request) {
  DCHECK_EQ(request.token.GetType(), HTMLToken::kStartTag);
  DCHECK(HasName(request.token, metaTag));

  return EraseAttributeIfInjected(request, http_equivAttr);
}

bool XSSAuditor::FilterBaseToken(const FilterTokenRequest& request) {
  DCHECK_EQ(request.token.GetType(), HTMLToken::kStartTag);
  DCHECK(HasName(request.token, baseTag));

  return EraseAttributeIfInjected(request, hrefAttr, String(),
                                  kSrcLikeAttributeTruncation);
}

bool XSSAuditor::FilterFormToken(const FilterTokenRequest& request) {
  DCHECK_EQ(request.token.GetType(), HTMLToken::kStartTag);
  DCHECK(HasName(request.token, formTag));

  return EraseAttributeIfInjected(request, actionAttr, kURLWithUniqueOrigin,
                                  kSrcLikeAttributeTruncation);
}

bool XSSAuditor::FilterInputToken(const FilterTokenRequest& request) {
  DCHECK_EQ(request.token.GetType(), HTMLToken::kStartTag);
  DCHECK(HasName(request.token, inputTag));

  return EraseAttributeIfInjected(request, formactionAttr, kURLWithUniqueOrigin,
                                  kSrcLikeAttributeTruncation);
}

bool XSSAuditor::FilterButtonToken(const FilterTokenRequest& request) {
  DCHECK_EQ(request.token.GetType(), HTMLToken::kStartTag);
  DCHECK(HasName(request.token, buttonTag));

  return EraseAttributeIfInjected(request, formactionAttr, kURLWithUniqueOrigin,
                                  kSrcLikeAttributeTruncation);
}

bool XSSAuditor::FilterLinkToken(const FilterTokenRequest& request) {
  DCHECK_EQ(request.token.GetType(), HTMLToken::kStartTag);
  DCHECK(HasName(request.token, linkTag));

  wtf_size_t index_of_attribute = 0;
  if (!FindAttributeWithName(request.token, relAttr, index_of_attribute))
    return false;

  const HTMLToken::Attribute& attribute =
      request.token.Attributes().at(index_of_attribute);
  LinkRelAttribute parsed_attribute(attribute.Value());
  if (!parsed_attribute.IsImport())
    return false;

  return EraseAttributeIfInjected(request, hrefAttr, kURLWithUniqueOrigin,
                                  kSrcLikeAttributeTruncation,
                                  kAllowSameOriginHref);
}

bool XSSAuditor::EraseDangerousAttributesIfInjected(
    const FilterTokenRequest& request) {
  bool did_block_script = false;
  for (wtf_size_t i = 0; i < request.token.Attributes().size(); ++i) {
    bool erase_attribute = false;
    bool value_contains_java_script_url = false;
    const HTMLToken::Attribute& attribute = request.token.Attributes().at(i);
    // FIXME: Don't create a new String for every attribute.value in the
    // document.
    if (IsNameOfInlineEventHandler(attribute.NameAsVector())) {
      erase_attribute = IsContainedInRequest(
          Canonicalize(SnippetFromAttribute(request, attribute),
                       kScriptLikeAttributeTruncation));
    } else if (IsSemicolonSeparatedAttribute(attribute)) {
      if (IsSemicolonSeparatedValueContainingJavaScriptURL(attribute.Value())) {
        value_contains_java_script_url = true;
        erase_attribute =
            IsContainedInRequest(Canonicalize(
                NameFromAttribute(request, attribute), kNoTruncation)) &&
            IsContainedInRequest(
                Canonicalize(SnippetFromAttribute(request, attribute),
                             kSemicolonSeparatedScriptLikeAttributeTruncation));
      }
    } else if (ProtocolIsJavaScript(
                   StripLeadingAndTrailingHTMLSpaces(attribute.Value()))) {
      value_contains_java_script_url = true;
      erase_attribute = IsContainedInRequest(
          Canonicalize(SnippetFromAttribute(request, attribute),
                       kScriptLikeAttributeTruncation));
    }
    if (!erase_attribute)
      continue;
    request.token.EraseValueOfAttribute(i);
    if (value_contains_java_script_url)
      request.token.AppendToAttributeValue(i, kSafeJavaScriptURL);
    did_block_script = true;
  }
  return did_block_script;
}

bool XSSAuditor::EraseAttributeIfInjected(const FilterTokenRequest& request,
                                          const QualifiedName& attribute_name,
                                          const String& replacement_value,
                                          TruncationKind treatment,
                                          HrefRestriction restriction) {
  wtf_size_t index_of_attribute = 0;
  if (!FindAttributeWithName(request.token, attribute_name, index_of_attribute))
    return false;

  const HTMLToken::Attribute& attribute =
      request.token.Attributes().at(index_of_attribute);
  if (!IsContainedInRequest(
          Canonicalize(SnippetFromAttribute(request, attribute), treatment)))
    return false;

  if (ThreadSafeMatch(attribute_name, srcAttr) ||
      (restriction == kAllowSameOriginHref &&
       ThreadSafeMatch(attribute_name, hrefAttr))) {
    if (IsLikelySafeResource(attribute.Value()))
      return false;
  } else if (ThreadSafeMatch(attribute_name, http_equivAttr)) {
    if (!IsDangerousHTTPEquiv(attribute.Value()))
      return false;
  }

  request.token.EraseValueOfAttribute(index_of_attribute);
  if (!replacement_value.IsEmpty())
    request.token.AppendToAttributeValue(index_of_attribute, replacement_value);

  return true;
}

String XSSAuditor::CanonicalizedSnippetForTagName(
    const FilterTokenRequest& request) {
  String source = request.source_tracker.SourceForToken(request.token);

  // TODO(tsepez): fix HTMLSourceTracker not to include NULs.
  // Beware that the source tracker may include leading NULs as part of
  // the souce for the token.
  unsigned start = 0;
  for (start = 0; start < source.length() && source[start] == '\0'; ++start)
    continue;

  // Grab a fixed number of characters equal to the length of the token's name
  // plus one (to account for the "<").
  return Canonicalize(
      source.Substring(start, request.token.GetName().size() + 1),
      kNoTruncation);
}

String XSSAuditor::NameFromAttribute(const FilterTokenRequest& request,
                                     const HTMLToken::Attribute& attribute) {
  // The range inlcudes the character which terminates the name. So,
  // for an input of |name="value"|, the snippet is |name=|.
  int start = attribute.NameRange().start - request.token.StartIndex();
  int end = attribute.ValueRange().start - request.token.StartIndex();
  return request.source_tracker.SourceForToken(request.token)
      .Substring(start, end - start);
}

String XSSAuditor::SnippetFromAttribute(const FilterTokenRequest& request,
                                        const HTMLToken::Attribute& attribute) {
  // The range doesn't include the character which terminates the value. So,
  // for an input of |name="value"|, the snippet is |name="value|. For a space
  // terminated unquoted input of |name=value |, the snippet is |name=value|.
  // Beware of empty unquoted values at the end of a token, we need to make sure
  // we don't clip off the equals-sign as there is no trailing space.
  // FIXME: We should grab one character before the name also.
  int name_start = attribute.NameRange().start - request.token.StartIndex();
  int value_start = attribute.ValueRange().start - request.token.StartIndex();
  int value_end = attribute.ValueRange().end - request.token.StartIndex();
  int length = value_end - name_start;
  if (value_start == value_end)
    length += 1;
  return request.source_tracker.SourceForToken(request.token)
      .Substring(name_start, length);
}

String XSSAuditor::Canonicalize(String snippet, TruncationKind treatment) {
  String decoded_snippet = FullyDecodeString(snippet, encoding_);

  if (treatment != kNoTruncation) {
    if (decoded_snippet.length() > kMaximumFragmentLengthTarget) {
      // Let the page influence the stopping point to avoid disclosing leading
      // fragments. Stop when we hit whitespace, since that is unlikely to be
      // part a leading fragment.
      wtf_size_t position = kMaximumFragmentLengthTarget;
      while (position < decoded_snippet.length() &&
             !IsHTMLSpace(decoded_snippet[position]))
        ++position;
      decoded_snippet.Truncate(position);
    }
    if (treatment == kSrcLikeAttributeTruncation)
      TruncateForSrcLikeAttribute(decoded_snippet);
    else if (treatment == kScriptLikeAttributeTruncation)
      TruncateForScriptLikeAttribute(decoded_snippet);
    else if (treatment == kSemicolonSeparatedScriptLikeAttributeTruncation)
      TruncateForSemicolonSeparatedScriptLikeAttribute(decoded_snippet);
  }

  return decoded_snippet.RemoveCharacters(&IsNonCanonicalCharacter);
}

String XSSAuditor::CanonicalizedSnippetForJavaScript(
    const FilterTokenRequest& request) {
  String string = request.source_tracker.SourceForToken(request.token);
  wtf_size_t start_position = 0;
  wtf_size_t end_position = string.length();
  wtf_size_t found_position = kNotFound;
  wtf_size_t last_non_space_position = kNotFound;

  // Skip over initial comments to find start of code.
  while (start_position < end_position) {
    while (start_position < end_position &&
           IsHTMLSpace<UChar>(string[start_position]))
      start_position++;

    // Under SVG/XML rules, only HTML comment syntax matters and the parser
    // returns these as a separate comment tokens. Having consumed whitespace,
    // we need not look further for these.
    if (request.should_allow_cdata)
      break;

    // Under HTML rules, both the HTML and JS comment synatx matters, and the
    // HTML comment ends at the end of the line, not with -->.
    if (StartsHTMLOpenCommentAt(string, start_position) ||
        StartsSingleLineCommentAt(string, start_position)) {
      while (start_position < end_position &&
             !IsJSNewline(string[start_position]))
        start_position++;
    } else if (StartsMultiLineCommentAt(string, start_position)) {
      if (start_position + 2 < end_position &&
          (found_position = string.Find("*/", start_position + 2)) != kNotFound)
        start_position = found_position + 2;
      else
        start_position = end_position;
    } else
      break;
  }

  String result;
  while (start_position < end_position && !result.length()) {
    // Stop at next comment (using the same rules as above for SVG/XML vs HTML),
    // when we encounter a comma, when we encounter a backtick, when we hit an
    // opening <script> tag, when we encounter a HTML closing comment, or when
    // we exceed the maximum length target.
    // - The comma rule covers a common parameter concatenation case performed
    //   by some web servers.
    // - The backtick rule covers the ECMA6 multi-line template string feature.
    // - The HTML closing comment rule covers the generous interpretation in
    //   https://tc39.github.io/ecma262/#prod-annexB-HTMLCloseComment.
    last_non_space_position = kNotFound;
    for (found_position = start_position; found_position < end_position;
         found_position++) {
      if (!request.should_allow_cdata) {
        if (StartsSingleLineCommentAt(string, found_position) ||
            StartsMultiLineCommentAt(string, found_position) ||
            StartsHTMLOpenCommentAt(string, found_position) ||
            StartsHTMLCloseCommentAt(string, found_position)) {
          break;
        }
      }
      if (string[found_position] == ',' || string[found_position] == '`')
        break;

      if (last_non_space_position != kNotFound &&
          (StartsOpeningScriptTagAt(string, found_position) ||
           StartsClosingScriptTagAt(string, found_position))) {
        found_position = last_non_space_position + 1;
        break;
      }
      if (found_position > start_position + kMaximumFragmentLengthTarget) {
        // After hitting the length target, we can only stop at a point where we
        // know we are not in the middle of a %-escape sequence. For the sake of
        // simplicity, approximate not stopping inside a (possibly multiply
        // encoded) %-escape sequence by breaking on whitespace only. We should
        // have enough text in these cases to avoid false positives.
        if (IsHTMLSpace<UChar>(string[found_position]))
          break;
      }
      if (!IsHTMLSpace<UChar>(string[found_position]))
        last_non_space_position = found_position;
    }
    result = Canonicalize(
        string.Substring(start_position, found_position - start_position),
        kNoTruncation);
    start_position = found_position + 1;
  }

  return result;
}

bool XSSAuditor::IsContainedInRequest(const String& decoded_snippet) {
  if (decoded_snippet.IsEmpty())
    return false;
  if (decoded_url_.FindIgnoringCase(decoded_snippet, 0) != kNotFound)
    return true;
  if (decoded_http_body_suffix_tree_ &&
      !decoded_http_body_suffix_tree_->MightContain(decoded_snippet))
    return false;
  return decoded_http_body_.FindIgnoringCase(decoded_snippet, 0) != kNotFound;
}

bool XSSAuditor::IsLikelySafeResource(const String& url) {
  // Give empty URLs and about:blank a pass. Making a resourceURL from an
  // empty string below will likely later fail the "no query args test" as
  // it inherits the document's query args.
  if (url.IsEmpty() || url == BlankURL().GetString())
    return true;

  // If the resource is loaded from the same host as the enclosing page, it's
  // probably not an XSS attack, so we reduce false positives by allowing the
  // request, ignoring scheme and port considerations. If the resource has a
  // query string, we're more suspicious, however, because that's pretty rare
  // and the attacker might be able to trick a server-side script into doing
  // something dangerous with the query string.
  if (document_url_.Host().IsEmpty())
    return false;

  KURL resource_url(document_url_, url);
  return (document_url_.Host() == resource_url.Host() &&
          resource_url.Query().IsEmpty());
}

bool XSSAuditor::IsSafeToSendToAnotherThread() const {
  return document_url_.IsSafeToSendToAnotherThread() &&
         decoded_url_.IsSafeToSendToAnotherThread() &&
         decoded_http_body_.IsSafeToSendToAnotherThread() &&
         http_body_as_string_.IsSafeToSendToAnotherThread();
}

}  // namespace blink
