// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/corb/corb_impl.h"

#include <stddef.h>

#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

using base::StringPiece;
using Decision = network::corb::ResponseAnalyzer::Decision;
using MimeType = network::corb::CrossOriginReadBlocking::MimeType;
using SniffingResult = network::corb::CrossOriginReadBlocking::SniffingResult;

namespace network::corb {

namespace {

// MIME types
const char kTextHtml[] = "text/html";
const char kTextXml[] = "text/xml";
const char kAppXml[] = "application/xml";
const char kAppJson[] = "application/json";
const char kImageSvg[] = "image/svg+xml";
const char kDashVideo[] = "application/dash+xml";  // https://crbug.com/947498
const char kTextJson[] = "text/json";
const char kTextPlain[] = "text/plain";

// Javascript MIME type suffixes for use in CORB protection logging. See also
// https://mimesniff.spec.whatwg.org/#javascript-mime-type.
const char* kJavaScriptSuffixes[] = {"ecmascript",
                                     "javascript",
                                     "x-ecmascript",
                                     "x-javascript",
                                     "javascript1.0",
                                     "javascript1.1",
                                     "javascript1.2",
                                     "javascript1.3",
                                     "javascript1.4",
                                     "javascript1.5",
                                     "jscript",
                                     "livescript",
                                     "js",
                                     "x-js"};

// TODO(lukasza): Remove kJsonProtobuf once this MIME type is not used in
// practice.  See also https://crbug.com/826756#c3
const char kJsonProtobuf[] = "application/json+protobuf";

// MIME type suffixes
const char kJsonSuffix[] = "+json";
const char kXmlSuffix[] = "+xml";

void AdvancePastWhitespace(StringPiece* data) {
  size_t offset = data->find_first_not_of(" \t\r\n");
  if (offset == base::StringPiece::npos) {
    // |data| was entirely whitespace.
    *data = StringPiece();
  } else {
    data->remove_prefix(offset);
  }
}

// Returns kYes if |data| starts with one of the string patterns in
// |signatures|, kMaybe if |data| is a prefix of one of the patterns in
// |signatures|, and kNo otherwise.
//
// When kYes is returned, the matching prefix is erased from |data|.
SniffingResult MatchesSignature(StringPiece* data,
                                const StringPiece signatures[],
                                size_t arr_size,
                                base::CompareCase compare_case) {
  for (size_t i = 0; i < arr_size; ++i) {
    if (signatures[i].length() <= data->length()) {
      if (base::StartsWith(*data, signatures[i], compare_case)) {
        // When |signatures[i]| is a prefix of |data|, it constitutes a match.
        // Strip the matching characters, and return.
        data->remove_prefix(signatures[i].length());
        return CrossOriginReadBlocking::kYes;
      }
    } else {
      if (base::StartsWith(signatures[i], *data, compare_case)) {
        // When |data| is a prefix of |signatures[i]|, that means that
        // subsequent bytes in the stream could cause a match to occur.
        return CrossOriginReadBlocking::kMaybe;
      }
    }
  }
  return CrossOriginReadBlocking::kNo;
}

size_t FindFirstJavascriptLineTerminator(const base::StringPiece& hay,
                                         size_t pos) {
  // https://www.ecma-international.org/ecma-262/8.0/index.html#prod-LineTerminator
  // defines LineTerminator ::= <LF> | <CR> | <LS> | <PS>.
  //
  // https://www.ecma-international.org/ecma-262/8.0/index.html#sec-line-terminators
  // defines <LF>, <CR>, <LS> ::= "\u2028", <PS> ::= "\u2029".
  //
  // In UTF8 encoding <LS> is 0xE2 0x80 0xA8 and <PS> is 0xE2 0x80 0xA9.
  while (true) {
    pos = hay.find_first_of("\n\r\xe2", pos);
    if (pos == base::StringPiece::npos)
      break;

    if (hay[pos] != '\xe2') {
      DCHECK(hay[pos] == '\r' || hay[pos] == '\n');
      break;
    }

    // TODO(lukasza): Prevent matching 3 bytes that span/straddle 2 UTF8
    // characters.
    base::StringPiece substr = hay.substr(pos);
    if (base::StartsWith(substr, "\u2028") ||
        base::StartsWith(substr, "\u2029"))
      break;

    pos++;  // Skip the \xe2 character.
  }
  return pos;
}

// Checks if |data| starts with an HTML comment (i.e. with "<!-- ... -->").
// - If there is a valid, terminated comment then returns kYes.
// - If there is a start of a comment, but the comment is not completed (e.g.
//   |data| == "<!-" or |data| == "<!-- not terminated yet") then returns
//   kMaybe.
// - Returns kNo otherwise.
//
// Mutates |data| to advance past the comment when returning kYes.  Note that
// SingleLineHTMLCloseComment ECMAscript rule is taken into account which means
// that characters following an HTML comment are consumed up to the nearest line
// terminating character.
SniffingResult MaybeSkipHtmlComment(StringPiece* data) {
  constexpr StringPiece kStartString = "<!--";
  if (!base::StartsWith(*data, kStartString)) {
    if (base::StartsWith(kStartString, *data))
      return CrossOriginReadBlocking::kMaybe;
    return CrossOriginReadBlocking::kNo;
  }

  constexpr StringPiece kEndString = "-->";
  size_t end_of_html_comment = data->find(kEndString, kStartString.length());
  if (end_of_html_comment == StringPiece::npos)
    return CrossOriginReadBlocking::kMaybe;
  end_of_html_comment += kEndString.length();

  // Skipping until the first line terminating character.  See
  // https://crbug.com/839945 for the motivation behind this.
  size_t end_of_line =
      FindFirstJavascriptLineTerminator(*data, end_of_html_comment);
  if (end_of_line == base::StringPiece::npos)
    return CrossOriginReadBlocking::kMaybe;

  // Found real end of the combined HTML/JS comment.
  data->remove_prefix(end_of_line);
  return CrossOriginReadBlocking::kYes;
}

// The function below returns a set of MIME types below may be blocked by CORB
// without any confirmation sniffing (in contrast to HTML/JSON/XML which require
// confirmation sniffing because images, scripts, etc. are frequently
// mislabelled by http servers as HTML/JSON/XML).
//
// CORB cannot block images, scripts, stylesheets and other resources that the
// web standards allows to be fetched in `no-cors` mode.  CORB cannot block
// these resources even if they are not explicitly labeled with their type - in
// practice http servers may serve images as application/octet-stream or even as
// text/html.  OTOH, CORB *can* block all Content-Types that are very unlikely
// to represent images, scripts, stylesheets, etc. - such Content-Types are
// returned by GetNeverSniffedMimeTypes.
//
// Some of the Content-Types returned below might seem like a layering violation
// (e.g. why would //services/network care about application/zip or
// application/pdf or application/msword), but note that the decision to list a
// Content-Type below is not driven by whether the type is handled above or
// below //services/network layer.  Instead the decision to list a Content-Type
// below is driven by whether the Content-Type is unlikely to be attached to an
// image, script, stylesheet or other subresource type that web standards
// require to be fetched in `no-cors` mode.  In particular, CORB would still
// want to prevent cross-site disclosure of "application/msword" even if Chrome
// did not support this type (AFAIK today this support is only present on
// ChromeOS) in one of Chrome's many layers.  Similarly, CORB wants to prevent
// disclosure of "application/zip" even though Chrome doesn't have built-in
// support for this resource type.  And CORB also wants to protect
// "application/pdf" even though Chrome happens to support this resource type.
const auto& GetNeverSniffedMimeTypes() {
  static constexpr auto kNeverSniffedMimeTypes = base::MakeFixedFlatSet<
      base::StringPiece>({
      // clang-format off
      // The types below (zip, protobuf, etc.) are based on most commonly used
      // content types according to HTTP Archive - see:
      // https://github.com/whatwg/fetch/issues/860#issuecomment-457330454
      "application/gzip",
      "application/x-gzip",
      "application/x-protobuf",
      "application/zip",
      "text/event-stream",
      // The types listed below were initially taken from the list of types
      // handled by MimeHandlerView (although we would want to protect them even
      // if Chrome didn't support rendering these content types and/or if there
      // was no such thing as MimeHandlerView).
      "application/msexcel",
      "application/mspowerpoint",
      "application/msword",
      "application/msword-template",
      "application/pdf",
      "application/vnd.ces-quickpoint",
      "application/vnd.ces-quicksheet",
      "application/vnd.ces-quickword",
      "application/vnd.ms-excel",
      "application/vnd.ms-excel.sheet.macroenabled.12",
      "application/vnd.ms-powerpoint",
      "application/vnd.ms-powerpoint.presentation.macroenabled.12",
      "application/vnd.ms-word",
      "application/vnd.ms-word.document.12",
      "application/vnd.ms-word.document.macroenabled.12",
      "application/vnd.msword",
      "application/"
          "vnd.openxmlformats-officedocument.presentationml.presentation",
      "application/"
          "vnd.openxmlformats-officedocument.presentationml.template",
      "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
      "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
      "application/"
          "vnd.openxmlformats-officedocument.wordprocessingml.document",
      "application/"
          "vnd.openxmlformats-officedocument.wordprocessingml.template",
      "application/vnd.presentation-openxml",
      "application/vnd.presentation-openxmlm",
      "application/vnd.spreadsheet-openxml",
      "application/vnd.wordprocessing-openxml",
      "text/csv",
      // Block signed documents to protect (potentially sensitive) unencrypted
      // body of the signed document.  There should be no need to block
      // encrypted documents (e.g. `multipart/encrypted` nor
      // `application/pgp-encrypted`) and no need to block the signatures (e.g.
      // `application/pgp-signature`).
      "multipart/signed",
      // Block multipart responses because a protected type (e.g. JSON) can
      // become multipart if returned in a range request with multiple parts.
      // This is compatible with the web because the renderer can only see into
      // the result of a fetch for a multipart file when the request is made
      // with CORS. Media tags only make single-range requests which will not
      // have the multipart type.
      "multipart/byteranges",
      // TODO(lukasza): https://crbug.com/802836#c11: Add
      // application/signed-exchange.
      // clang-format on
  });

  // All items need to be lower-case, to support case-insensitive comparisons
  // later.
  DCHECK(base::ranges::all_of(kNeverSniffedMimeTypes, [](const auto& s) {
    return s == base::ToLowerASCII(s);
  }));

  return kNeverSniffedMimeTypes;
}

void LogAction(CrossOriginReadBlocking::Action action) {
  UMA_HISTOGRAM_ENUMERATION("SiteIsolation.XSD.Browser.Action", action);
}

}  // namespace

// static
bool CrossOriginReadBlocking::IsJavascriptMimeType(
    base::StringPiece mime_type) {
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  for (const std::string& suffix : kJavaScriptSuffixes) {
    if (base::EndsWith(mime_type, suffix, kCaseInsensitive))
      return true;
  }

  return false;
}

// static
MimeType CrossOriginReadBlocking::GetCanonicalMimeType(
    base::StringPiece mime_type) {
  // Checking for image/svg+xml and application/dash+xml early ensures that they
  // won't get classified as MimeType::kXml by the presence of the "+xml"
  // suffix.
  if (base::EqualsCaseInsensitiveASCII(mime_type, kImageSvg) ||
      base::EqualsCaseInsensitiveASCII(mime_type, kDashVideo))
    return MimeType::kOthers;

  // See also https://mimesniff.spec.whatwg.org/#html-mime-type
  if (base::EqualsCaseInsensitiveASCII(mime_type, kTextHtml))
    return MimeType::kHtml;

  // See also https://mimesniff.spec.whatwg.org/#json-mime-type
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  if (base::EqualsCaseInsensitiveASCII(mime_type, kAppJson) ||
      base::EqualsCaseInsensitiveASCII(mime_type, kTextJson) ||
      base::EqualsCaseInsensitiveASCII(mime_type, kJsonProtobuf) ||
      base::EndsWith(mime_type, kJsonSuffix, kCaseInsensitive)) {
    return MimeType::kJson;
  }

  // See also https://mimesniff.spec.whatwg.org/#xml-mime-type
  if (base::EqualsCaseInsensitiveASCII(mime_type, kAppXml) ||
      base::EqualsCaseInsensitiveASCII(mime_type, kTextXml) ||
      base::EndsWith(mime_type, kXmlSuffix, kCaseInsensitive)) {
    return MimeType::kXml;
  }

  if (base::EqualsCaseInsensitiveASCII(mime_type, kTextPlain))
    return MimeType::kPlain;

  if (base::Contains(GetNeverSniffedMimeTypes(),
                     base::ToLowerASCII(mime_type))) {
    return MimeType::kNeverSniffed;
  }

  return MimeType::kOthers;
}

// static
// This function is a slight modification of |net::SniffForHTML|.
SniffingResult CrossOriginReadBlocking::SniffForHTML(StringPiece data) {
  // The content sniffers used by Chrome and Firefox are using "<!--" as one of
  // the HTML signatures, but it also appears in valid JavaScript, considered as
  // well-formed JS by the browser.  Since we do not want to block any JS, we
  // exclude it from our HTML signatures. This can weaken our CORB policy,
  // but we can break less websites.
  //
  // Note that <body> and <br> are not included below, since <b is a prefix of
  // them.
  //
  // TODO(dsjang): parameterize |net::SniffForHTML| with an option that decides
  // whether to include <!-- or not, so that we can remove this function.
  // TODO(dsjang): Once CrossOriginReadBlocking is moved into the browser
  // process, we should do single-thread checking here for the static
  // initializer.
  static constexpr StringPiece kHtmlSignatures[] = {
      StringPiece("<!doctype html"),  // HTML5 spec
      StringPiece("<script"),         // HTML5 spec, Mozilla
      StringPiece("<html"),           // HTML5 spec, Mozilla
      StringPiece("<head"),           // HTML5 spec, Mozilla
      StringPiece("<iframe"),         // Mozilla
      StringPiece("<h1"),             // Mozilla
      StringPiece("<div"),            // Mozilla
      StringPiece("<font"),           // Mozilla
      StringPiece("<table"),          // Mozilla
      StringPiece("<a"),              // Mozilla
      StringPiece("<style"),          // Mozilla
      StringPiece("<title"),          // Mozilla
      StringPiece("<b"),              // Mozilla (note: subsumes <body>, <br>)
      StringPiece("<p")               // Mozilla
  };

  while (data.length() > 0) {
    AdvancePastWhitespace(&data);

    SniffingResult signature_match =
        MatchesSignature(&data, kHtmlSignatures, std::size(kHtmlSignatures),
                         base::CompareCase::INSENSITIVE_ASCII);
    if (signature_match != kNo)
      return signature_match;

    SniffingResult comment_match = MaybeSkipHtmlComment(&data);
    if (comment_match != kYes)
      return comment_match;
  }

  // All of |data| was consumed, without a clear determination.
  return kMaybe;
}

// static
SniffingResult CrossOriginReadBlocking::SniffForXML(base::StringPiece data) {
  // TODO(dsjang): Once CrossOriginReadBlocking is moved into the browser
  // process, we should do single-thread checking here for the static
  // initializer.
  AdvancePastWhitespace(&data);
  static constexpr StringPiece kXmlSignatures[] = {StringPiece("<?xml")};
  return MatchesSignature(&data, kXmlSignatures, std::size(kXmlSignatures),
                          base::CompareCase::SENSITIVE);
}

// static
SniffingResult CrossOriginReadBlocking::SniffForJSON(base::StringPiece data) {
  // Currently this function looks for an opening brace ('{'), followed by a
  // double-quoted string literal, followed by a colon. Importantly, such a
  // sequence is a Javascript syntax error: although the JSON object syntax is
  // exactly Javascript's object-initializer syntax, a Javascript object-
  // initializer expression is not valid as a standalone Javascript statement.
  //
  // TODO(nick): We have to come up with a better way to sniff JSON. The
  // following are known limitations of this function:
  // https://crbug.com/795470/ Support non-dictionary values (e.g. lists)
  enum {
    kStartState,
    kLeftBraceState,
    kLeftQuoteState,
    kEscapeState,
    kRightQuoteState,
  } state = kStartState;

  for (size_t i = 0; i < data.length(); ++i) {
    const char c = data[i];
    if (state != kLeftQuoteState && state != kEscapeState) {
      // Whitespace is ignored (outside of string literals)
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        continue;
    }

    switch (state) {
      case kStartState:
        if (c == '{')
          state = kLeftBraceState;
        else
          return kNo;
        break;
      case kLeftBraceState:
        if (c == '"')
          state = kLeftQuoteState;
        else
          return kNo;
        break;
      case kLeftQuoteState:
        if (c == '"')
          state = kRightQuoteState;
        else if (c == '\\')
          state = kEscapeState;
        break;
      case kEscapeState:
        // Simplification: don't bother rejecting hex escapes.
        state = kLeftQuoteState;
        break;
      case kRightQuoteState:
        if (c == ':')
          return kYes;
        return kNo;
    }
  }
  return kMaybe;
}

// static
SniffingResult CrossOriginReadBlocking::SniffForFetchOnlyResource(
    base::StringPiece data) {
  // kScriptBreakingPrefixes contains prefixes that are conventionally used to
  // prevent a JSON response from becoming a valid Javascript program (an attack
  // vector known as XSSI). The presence of such a prefix is a strong signal
  // that the resource is meant to be consumed only by the fetch API or
  // XMLHttpRequest, and is meant to be protected from use in non-CORS, cross-
  // origin contexts like <script>, <img>, etc.
  //
  // These prefixes work either by inducing a syntax error, or inducing an
  // infinite loop. In either case, the prefix must create a guarantee that no
  // matter what bytes follow it, the entire response would be worthless to
  // execute as a <script>.
  static constexpr StringPiece kScriptBreakingPrefixes[] = {
      // Parser breaker prefix.
      //
      // Built into angular.js (followed by a comma and a newline):
      //   https://docs.angularjs.org/api/ng/service/$http
      //
      // Built into the Java Spring framework (followed by a comma and a space):
      //   https://goo.gl/xP7FWn
      //
      // Observed on google.com (without a comma, followed by a newline).
      StringPiece(")]}'"),

      // Apache struts: https://struts.apache.org/plugins/json/#prefix
      StringPiece("{}&&"),

      // Spring framework (historically): https://goo.gl/JYPFAv
      StringPiece("{} &&"),

      // Infinite loops.
      StringPiece("for(;;);"),  // observed on facebook.com
      StringPiece("while(1);"),
      StringPiece("for (;;);"),
      StringPiece("while (1);"),
  };
  SniffingResult has_parser_breaker = MatchesSignature(
      &data, kScriptBreakingPrefixes, std::size(kScriptBreakingPrefixes),
      base::CompareCase::SENSITIVE);
  if (has_parser_breaker != kNo)
    return has_parser_breaker;

  // A non-empty JSON object also effectively introduces a JS syntax error.
  return SniffForJSON(data);
}

// An interface to enable incremental content sniffing. These are instantiated
// for each each request; thus they can be stateful.
class CrossOriginReadBlocking::CorbResponseAnalyzer::ConfirmationSniffer {
 public:
  virtual ~ConfirmationSniffer() = default;

  // Called after data is read from the network. |sniffing_buffer| contains the
  // entire response body delivered thus far.
  virtual void OnDataAvailable(base::StringPiece sniffing_buffer) = 0;

  // Returns true if the return value of IsConfirmedContentType() might change
  // with the addition of more data. Returns false if a final decision is
  // available.
  virtual bool WantsMoreData() const = 0;

  // Returns true if the data has been confirmed to be of the CORB-protected
  // content type that this sniffer is intended to detect.
  virtual bool IsConfirmedContentType() const = 0;
};

// A ConfirmationSniffer that wraps one of the sniffing functions from
// CrossOriginReadBlocking.
class CrossOriginReadBlocking::CorbResponseAnalyzer::SimpleConfirmationSniffer
    : public CrossOriginReadBlocking::CorbResponseAnalyzer::
          ConfirmationSniffer {
 public:
  // The function pointer type corresponding to one of the available sniffing
  // functions from CrossOriginReadBlocking.
  using SnifferFunction = decltype(&CrossOriginReadBlocking::SniffForHTML);

  explicit SimpleConfirmationSniffer(SnifferFunction sniffer_function)
      : sniffer_function_(sniffer_function) {}
  ~SimpleConfirmationSniffer() override = default;

  SimpleConfirmationSniffer(const SimpleConfirmationSniffer*) = delete;
  SimpleConfirmationSniffer& operator=(const SimpleConfirmationSniffer*) =
      delete;

  void OnDataAvailable(base::StringPiece sniffing_buffer) final {
    // The sniffing functions don't support streaming, so with each new chunk of
    // data, call the sniffer on the whole buffer.
    last_sniff_result_ = (*sniffer_function_)(sniffing_buffer);
  }

  bool WantsMoreData() const final {
    // kNo and kYes results are final, meaning that sniffing can stop once they
    // occur. A kMaybe result corresponds to an indeterminate state, that could
    // change to kYes or kNo with more data.
    return last_sniff_result_ == SniffingResult::kMaybe;
  }

  bool IsConfirmedContentType() const final {
    // Only confirm the mime type if an affirmative pattern (e.g. an HTML tag,
    // if using the HTML sniffer) was detected.
    //
    // Note that if the stream ends (or net::kMaxBytesToSniff has been reached)
    // and |last_sniff_result_| is kMaybe, the response is allowed to go
    // through.
    return last_sniff_result_ == SniffingResult::kYes;
  }

 private:
  // The function that actually knows how to sniff for a content type.
  SnifferFunction sniffer_function_;

  // Result of sniffing the data available thus far.
  SniffingResult last_sniff_result_ = SniffingResult::kMaybe;
};

Decision CrossOriginReadBlocking::CorbResponseAnalyzer::Init(
    const GURL& request_url,
    const absl::optional<url::Origin>& request_initiator,
    mojom::RequestMode request_mode,
    const mojom::URLResponseHead& response) {
  seems_sensitive_from_cors_heuristic_ =
      SeemsSensitiveFromCORSHeuristic(response);
  seems_sensitive_from_cache_heuristic_ =
      SeemsSensitiveFromCacheHeuristic(response);
  supports_range_requests_ = SupportsRangeRequests(response);
  has_nosniff_header_ = HasNoSniff(response);
  content_length_ = response.content_length;
  http_response_code_ =
      response.headers ? response.headers->response_code() : 0;

  LogAction(Action::kResponseStarted);

  // CORB should look directly at the Content-Type header if one has been
  // received from the network. Ignoring |response.mime_type| helps avoid
  // breaking legitimate websites (which might happen more often when blocking
  // would be based on the mime type sniffed by MimeSniffingResourceHandler).
  //
  // This value could be computed later in ShouldBlockBasedOnHeaders after
  // has_nosniff_header, but we compute it here to keep
  // ShouldBlockBasedOnHeaders (which is called twice) const.
  //
  // TODO(nick): What if the mime type is omitted? Should that be treated the
  // same as text/plain? https://crbug.com/795971
  std::string mime_type;
  if (response.headers)
    response.headers->GetMimeType(&mime_type);
  // Canonicalize the MIME type.  Note that even if it doesn't claim to be a
  // blockable type (i.e., HTML, XML, JSON, or plain text), it may still fail
  // the checks during the SniffForFetchOnlyResource() phase.
  canonical_mime_type_ = GetCanonicalMimeType(mime_type);

  should_block_based_on_headers_ =
      ShouldBlockBasedOnHeaders(request_mode, request_url, request_initiator,
                                response, canonical_mime_type_);

  // Check if the response seems sensitive and if so include in our CORB
  // protection logging. We have not sniffed yet, so the answer might be
  // kSniffMore.
  if (seems_sensitive_from_cors_heuristic_ ||
      seems_sensitive_from_cache_heuristic_) {
    // Create a new Origin with a unique internal identifier so we can pretend
    // the request is cross-origin.
    url::Origin cross_origin_request_initiator = url::Origin();
    // kNoCors is used (instead of passing `request_mode`) to also cover CORS
    // requests with the CORB Protection heuristics and UMAs.  Using kNoCors
    // simulates an attacker requesting "seems-sensitive" subresources from a
    // script tag.
    Decision would_protect_based_on_headers = ShouldBlockBasedOnHeaders(
        mojom::RequestMode::kNoCors, request_url,
        cross_origin_request_initiator, response, canonical_mime_type_);
    corb_protection_logging_needs_sniffing_ =
        (would_protect_based_on_headers == Decision::kSniffMore);
    hypothetical_sniffing_mode_ =
        corb_protection_logging_needs_sniffing_ &&
        should_block_based_on_headers_ != Decision::kSniffMore;
    mime_type_bucket_ = GetMimeTypeBucket(response);
    UMA_HISTOGRAM_BOOLEAN("SiteIsolation.CORBProtection.SensitiveResource",
                          true);
    if (!corb_protection_logging_needs_sniffing_) {
      // If we are not going to sniff, then we can and must log everything now.
      LogSensitiveResponseProtection(
          BlockingDecisionToProtectionDecision(would_protect_based_on_headers));
    }
  } else {
    UMA_HISTOGRAM_BOOLEAN("SiteIsolation.CORBProtection.SensitiveResource",
                          false);
  }
  if (needs_sniffing())
    CreateSniffers();

  return GetCorbDecision();
}

CrossOriginReadBlocking::CorbResponseAnalyzer::CorbResponseAnalyzer() = default;

CrossOriginReadBlocking::CorbResponseAnalyzer::~CorbResponseAnalyzer() {
  if (ShouldBlock()) {
    LogBlockedResponse();
  } else {
    // Allowing happens either 1) explicitly, or 2) when sniffing didn't reach a
    // conclusion after sniffing 1024 (or all) bytes.
    DCHECK(ShouldAllow() || needs_sniffing());
    LogAllowedResponse();
  }
}

// static
Decision
CrossOriginReadBlocking::CorbResponseAnalyzer::ShouldBlockBasedOnHeaders(
    mojom::RequestMode request_mode,
    const GURL& request_url,
    const absl::optional<url::Origin>& request_initiator,
    const mojom::URLResponseHead& response,
    MimeType canonical_mime_type) {
  // The checks in this method are ordered to rule out blocking in most cases as
  // quickly as possible.  Checks that are likely to lead to returning false or
  // that are inexpensive should be near the top.

  // Extract the `initiator` of the request, allowing requests with no
  // initiator.  (Such requests are browser-initiated and therefore trustworthy;
  // CorsURLLoaderFactory::IsValidRequest enforces that renderer-initiated
  // requests specify a non-null `request_initiator`.)
  if (!request_initiator.has_value())
    return Decision::kAllow;
  const url::Origin& initiator = request_initiator.value();

  // Don't block same-origin documents.
  if (initiator.IsSameOriginWith(request_url))
    return Decision::kAllow;

  // Only apply CORB to `no-cors` requests.
  //
  // CORB doesn't need to block kNavigate requests because results of these are
  // OOPIF-isolated (note that CorsURLLoaderFactory::IsValidRequest
  // validates that only the Browser process can initiate requests in kNavigate
  // mode).
  //
  // CORB doesn't need to work with kSameOrigin, kCors, nor
  // kCorsWithForcedPreflight modes, because these are covered by OOR-CORS.
  if (request_mode != mojom::RequestMode::kNoCors)
    return Decision::kAllow;

  // Requests from foo.example.com will consult foo.example.com's service worker
  // first (if one has been registered).  The service worker can handle requests
  // initiated by foo.example.com even if they are cross-origin (e.g. requests
  // for bar.example.com).  This is okay, because there is no security boundary
  // between foo.example.com and the service worker of foo.example.com + because
  // the response data is "conjured" within the service worker of
  // foo.example.com (rather than being fetched from bar.example.com).
  // Therefore such responses should not be blocked by CORB, unless the
  // initiator opted out of CORS / opted into receiving an opaque response.  See
  // also https://crbug.com/803672.
  if (response.was_fetched_via_service_worker) {
    switch (response.response_type) {
      case mojom::FetchResponseType::kBasic:
      case mojom::FetchResponseType::kCors:
      case mojom::FetchResponseType::kDefault:
      case mojom::FetchResponseType::kError:
        // Non-opaque responses shouldn't be blocked.
        return Decision::kAllow;
      case mojom::FetchResponseType::kOpaque:
      case mojom::FetchResponseType::kOpaqueRedirect:
        // Opaque responses are eligible for blocking. Continue on...
        break;
    }
  }

  // Some types (e.g. ZIP) are protected without any confirmation sniffing.
  if (canonical_mime_type == MimeType::kNeverSniffed)
    return Decision::kBlock;

  // If this is a partial response, sniffing is not possible, so allow the
  // response if it's not a protected mime type.
  std::string range_header;
  response.headers->GetNormalizedHeader("content-range", &range_header);
  bool has_range_header = !range_header.empty();
  if (has_range_header) {
    switch (canonical_mime_type) {
      case MimeType::kOthers:
      case MimeType::kPlain:  // See also https://crbug.com/801709
        return Decision::kAllow;
      case MimeType::kHtml:
      case MimeType::kJson:
      case MimeType::kXml:
        return Decision::kBlock;
      case MimeType::kInvalidMimeType:
      case MimeType::kNeverSniffed:  // Handled much earlier.
        NOTREACHED();
        return Decision::kBlock;
    }
  }

  // We intend to block the response at this point.  However, we will usually
  // sniff the contents to confirm the MIME type, to avoid blocking incorrectly
  // labeled JavaScript, JSONP, etc files.
  //
  // Note: if there is a nosniff header, it means we should honor the response
  // mime type without trying to confirm it.
  //
  // Decide whether to block based on the MIME type.
  switch (canonical_mime_type) {
    case MimeType::kHtml:
    case MimeType::kXml:
    case MimeType::kJson:
    case MimeType::kPlain:
      if (HasNoSniff(response))
        return Decision::kBlock;
      return Decision::kSniffMore;

    case MimeType::kOthers:
      // Stylesheets shouldn't be sniffed for JSON parser breakers - see
      // https://crbug.com/809259.
      if (base::EqualsCaseInsensitiveASCII(response.mime_type, "text/css"))
        return Decision::kAllow;
      return Decision::kSniffMore;

    case MimeType::kInvalidMimeType:
    case MimeType::kNeverSniffed:  // Handled much earlier.
      NOTREACHED();
      return Decision::kBlock;
  }
  NOTREACHED();
  return Decision::kBlock;
}

// static
bool CrossOriginReadBlocking::CorbResponseAnalyzer::
    SeemsSensitiveFromCORSHeuristic(const mojom::URLResponseHead& response) {
  // Check if the response has an Access-Control-Allow-Origin with a value other
  // than "*" or "null" ("null" offers no more protection than "*" because it
  // matches any unique origin).
  if (!response.headers)
    return false;
  std::string cors_header_value;
  response.headers->GetNormalizedHeader("access-control-allow-origin",
                                        &cors_header_value);
  if (cors_header_value != "*" && cors_header_value != "null" &&
      cors_header_value != "") {
    return true;
  }
  return false;
}

// static
bool CrossOriginReadBlocking::CorbResponseAnalyzer::
    SeemsSensitiveFromCacheHeuristic(const mojom::URLResponseHead& response) {
  // Check if the response has both Vary: Origin and Cache-Control: Private
  // headers, which we take as a signal that it may be a sensitive resource. We
  // require both to reduce the number of false positives (as both headers are
  // sometimes used on non-sensitive resources). Cache-Control: no-store appears
  // on non-sensitive resources that change frequently, so we ignore it here.
  if (!response.headers)
    return false;
  bool has_vary_origin = response.headers->HasHeaderValue("vary", "origin");
  bool has_cache_private =
      response.headers->HasHeaderValue("cache-control", "private");
  return has_vary_origin && has_cache_private;
}

// static
bool CrossOriginReadBlocking::CorbResponseAnalyzer::SupportsRangeRequests(
    const mojom::URLResponseHead& response) {
  if (response.headers) {
    std::string value;
    response.headers->GetNormalizedHeader("accept-ranges", &value);
    if (!value.empty() && !base::EqualsCaseInsensitiveASCII(value, "none")) {
      return true;
    }
  }
  return false;
}

// static
CrossOriginReadBlocking::CorbResponseAnalyzer::MimeTypeBucket
CrossOriginReadBlocking::CorbResponseAnalyzer::GetMimeTypeBucket(
    const mojom::URLResponseHead& response) {
  std::string mime_type;
  if (response.headers)
    response.headers->GetMimeType(&mime_type);
  MimeType canonical_mime_type = GetCanonicalMimeType(mime_type);
  switch (canonical_mime_type) {
    case MimeType::kHtml:
    case MimeType::kXml:
    case MimeType::kJson:
    case MimeType::kNeverSniffed:
    case MimeType::kPlain:
      return kProtected;
    case MimeType::kOthers:
      break;
    case MimeType::kInvalidMimeType:
      NOTREACHED();
      break;
  }

  // Javascript is assumed public. See also
  // https://mimesniff.spec.whatwg.org/#javascript-mime-type.
  if (IsJavascriptMimeType(mime_type)) {
    return kPublic;
  }

  // Images are assumed public. See also
  // https://mimesniff.spec.whatwg.org/#image-mime-type.
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  if (base::StartsWith(mime_type, "image", kCaseInsensitive)) {
    return kPublic;
  }

  // Audio and video are assumed public. See also
  // https://mimesniff.spec.whatwg.org/#audio-or-video-mime-type.
  if (base::StartsWith(mime_type, "audio", kCaseInsensitive) ||
      base::StartsWith(mime_type, "video", kCaseInsensitive) ||
      base::EqualsCaseInsensitiveASCII(mime_type, "application/ogg") ||
      base::EqualsCaseInsensitiveASCII(mime_type, "application/dash+xml")) {
    return kPublic;
  }

  // CSS files are assumed public and must be sent with text/css.
  if (base::EqualsCaseInsensitiveASCII(mime_type, "text/css")) {
    return kPublic;
  }
  return kOther;
}

void CrossOriginReadBlocking::CorbResponseAnalyzer::CreateSniffers() {
  // Create one or more |sniffers_| to confirm that the body is actually the
  // MIME type advertised in the Content-Type header.
  DCHECK(needs_sniffing());
  DCHECK(sniffers_.empty());

  // When the MIME type is "text/plain", create sniffers for HTML, XML and
  // JSON. If any of these sniffers match, the response will be blocked.
  const bool use_all = canonical_mime_type_ == MimeType::kPlain;

  // HTML sniffer.
  if (use_all || canonical_mime_type_ == MimeType::kHtml) {
    sniffers_.push_back(std::make_unique<SimpleConfirmationSniffer>(
        &CrossOriginReadBlocking::SniffForHTML));
  }

  // XML sniffer.
  if (use_all || canonical_mime_type_ == MimeType::kXml) {
    sniffers_.push_back(std::make_unique<SimpleConfirmationSniffer>(
        &CrossOriginReadBlocking::SniffForXML));
  }

  // JSON sniffer.
  if (use_all || canonical_mime_type_ == MimeType::kJson) {
    sniffers_.push_back(std::make_unique<SimpleConfirmationSniffer>(
        &CrossOriginReadBlocking::SniffForJSON));
  }

  // Parser-breaker sniffer.
  //
  // Because these prefixes are an XSSI-defeating mechanism, CORB considers
  // them distinctive enough to be worth blocking no matter the Content-Type
  // header. So this sniffer is created unconditionally.
  //
  // For MimeType::kOthers, this will be the only sniffer that's active.
  sniffers_.push_back(std::make_unique<SimpleConfirmationSniffer>(
      &CrossOriginReadBlocking::SniffForFetchOnlyResource));
}

Decision CrossOriginReadBlocking::CorbResponseAnalyzer::Sniff(
    base::StringPiece data) {
  DCHECK(needs_sniffing());
  DCHECK(!sniffers_.empty());
  DCHECK(!found_blockable_content_);

  DCHECK_LE(data.size(), static_cast<size_t>(net::kMaxBytesToSniff));

  for (size_t i = 0; i < sniffers_.size();) {
    sniffers_[i]->OnDataAvailable(data);

    if (sniffers_[i]->WantsMoreData()) {
      i++;
      continue;
    }

    if (sniffers_[i]->IsConfirmedContentType()) {
      found_blockable_content_ = true;
      sniffers_.clear();
      break;
    } else {
      // This response is CORB-exempt as far as this sniffer is concerned;
      // remove it from the list.
      sniffers_.erase(sniffers_.begin() + i);
    }
  }

  return GetCorbDecision();
}

Decision CrossOriginReadBlocking::CorbResponseAnalyzer::
    HandleEndOfSniffableResponseBody() {
  // If CORB reached the end of sniffable response body, then it means that the
  // HTML, XML, and JSON confirmation sniffers weren't able to confirm that the
  // response body contains HTML, XML, or JSON.  In this case CORB fails open,
  // by assuming that the response body might contain an allowed resource (e.g.
  // an image, or a script).
  return Decision::kAllow;
}

bool CrossOriginReadBlocking::CorbResponseAnalyzer::ShouldAllow() const {
  // If we're in hypothetical mode then CORB must have decided to kAllow (see
  // comment in ShouldBlock). Thus we just need to wait until the sniffers are
  // all done (i.e. empty).
  if (hypothetical_sniffing_mode_) {
    DCHECK_EQ(should_block_based_on_headers_, Decision::kAllow);
    return sniffers_.empty();
  }
  switch (should_block_based_on_headers_) {
    case Decision::kAllow:
      return true;
    case Decision::kSniffMore:
      return sniffers_.empty() && !found_blockable_content_;
    case Decision::kBlock:
      return false;
  }
}

bool CrossOriginReadBlocking::CorbResponseAnalyzer::ShouldBlock() const {
  // If we're in *hypothetical* sniffing mode then the following must be true:
  // (1) We are only sniffing to find out if CORB would have blocked the request
  // were it made cross origin (CORB itself did *not* need to sniff the file).
  // (2) CORB must have decided to kAllow (if it was kBlock then the protection
  // decision would have been kBlock as well, no hypothetical mode needed).
  if (hypothetical_sniffing_mode_) {
    DCHECK_EQ(should_block_based_on_headers_, Decision::kAllow);
    return false;
  }
  switch (should_block_based_on_headers_) {
    case Decision::kAllow:
      return false;
    case Decision::kSniffMore:
      return sniffers_.empty() && found_blockable_content_;
    case Decision::kBlock:
      return true;
  }
}

bool CrossOriginReadBlocking::CorbResponseAnalyzer::
    ShouldReportBlockedResponse() const {
  if (!ShouldBlock())
    return false;

  // Don't bother showing a warning message when blocking responses that are
  // already empty.
  if (content_length_ == 0)
    return false;
  if (http_response_code_ == 204)
    return false;

  // Don't bother showing a warning message when blocking responses that are
  // associated with error responses (e.g. it is quite common to serve a
  // text/html 404 error page for an <img> tag pointing to a wrong URL).
  if (400 <= http_response_code_ && http_response_code_ <= 599)
    return false;

  return true;
}

ResponseAnalyzer::BlockedResponseHandling
CrossOriginReadBlocking::CorbResponseAnalyzer::ShouldHandleBlockedResponseAs()
    const {
  // CORB wants blocked responses to be empty responses.
  return ResponseAnalyzer::BlockedResponseHandling::kEmptyResponse;
}

Decision CrossOriginReadBlocking::CorbResponseAnalyzer::GetCorbDecision() {
  if (ShouldBlock())
    return Decision::kBlock;
  else if (ShouldAllow())
    return Decision::kAllow;
  else
    return Decision::kSniffMore;
}

void CrossOriginReadBlocking::CorbResponseAnalyzer::LogAllowedResponse() {
  DCHECK(!has_logged_final_decision_);
  has_logged_final_decision_ = true;

  if (corb_protection_logging_needs_sniffing_) {
    LogSensitiveResponseProtection(
        SniffingDecisionToProtectionDecision(found_blockable_content_));
  }
  // Note that if a response is allowed because of hitting EOF or
  // kMaxBytesToSniff, then |sniffers_| are not emptied and consequently
  // ShouldAllow doesn't start returning true.  This means that we can't
  // DCHECK(ShouldAllow()) or DCHECK(sniffers_.empty()) here - the decision to
  // allow the response could have been made in the
  // CrossSiteDocumentResourceHandler layer without CrossOriginReadBlocking
  // realizing that it has hit EOF or kMaxBytesToSniff.

  // Note that the response might be allowed even if ShouldBlock() returns true
  // - for example to allow responses to requests initiated by content scripts.
  // This means that we cannot DCHECK(!ShouldBlock()) here.

  LogAction(needs_sniffing() ? Action::kAllowedAfterSniffing
                             : Action::kAllowedWithoutSniffing);
}

void CrossOriginReadBlocking::CorbResponseAnalyzer::LogBlockedResponse() {
  DCHECK(!has_logged_final_decision_);
  has_logged_final_decision_ = true;

  DCHECK(!ShouldAllow());
  DCHECK(ShouldBlock());
  DCHECK(sniffers_.empty());

  if (corb_protection_logging_needs_sniffing_) {
    LogSensitiveResponseProtection(
        SniffingDecisionToProtectionDecision(found_blockable_content_));
  }

  LogAction(needs_sniffing() ? Action::kBlockedAfterSniffing
                             : Action::kBlockedWithoutSniffing);

  UMA_HISTOGRAM_ENUMERATION(
      "SiteIsolation.XSD.Browser.Blocked.CanonicalMimeType",
      canonical_mime_type_);
}

// static
bool CrossOriginReadBlocking::CorbResponseAnalyzer::HasNoSniff(
    const mojom::URLResponseHead& response) {
  if (!response.headers)
    return false;
  std::string nosniff_header;
  response.headers->GetNormalizedHeader("x-content-type-options",
                                        &nosniff_header);
  return base::EqualsCaseInsensitiveASCII(nosniff_header, "nosniff");
}

// static
CrossOriginReadBlocking::CorbResponseAnalyzer::CrossOriginProtectionDecision
CrossOriginReadBlocking::CorbResponseAnalyzer::
    BlockingDecisionToProtectionDecision(Decision blocking_decision) {
  switch (blocking_decision) {
    case Decision::kAllow:
      return CrossOriginProtectionDecision::kAllow;
    case Decision::kBlock:
      return CrossOriginProtectionDecision::kBlock;
    case Decision::kSniffMore:
      return CrossOriginProtectionDecision::kNeedToSniffMore;
  }
}

// static
CrossOriginReadBlocking::CorbResponseAnalyzer::CrossOriginProtectionDecision
CrossOriginReadBlocking::CorbResponseAnalyzer::
    SniffingDecisionToProtectionDecision(bool found_blockable_content) {
  if (found_blockable_content)
    return CrossOriginProtectionDecision::kBlockedAfterSniffing;
  return CrossOriginProtectionDecision::kAllowedAfterSniffing;
}

void CrossOriginReadBlocking::CorbResponseAnalyzer::
    LogSensitiveResponseProtection(
        CrossOriginProtectionDecision protection_decision) const {
  DCHECK(seems_sensitive_from_cors_heuristic_ ||
         seems_sensitive_from_cache_heuristic_);
  if (seems_sensitive_from_cors_heuristic_) {
    switch (mime_type_bucket_) {
      case kProtected:
        UMA_HISTOGRAM_ENUMERATION(
            "SiteIsolation.CORBProtection.CORSHeuristic.ProtectedMimeType",
            protection_decision);
        // We report if a response with a protected MIME type supports range
        // requests since we want to measure how often making a multipart range
        // requests would have allowed bypassing CORB.
        if (protection_decision == CrossOriginProtectionDecision::kBlock) {
          UMA_HISTOGRAM_BOOLEAN(
              "SiteIsolation.CORBProtection.CORSHeuristic.ProtectedMimeType."
              "BlockedWithRangeSupport",
              supports_range_requests_);
          UMA_HISTOGRAM_BOOLEAN(
              "SiteIsolation.CORBProtection.CORSHeuristic.ProtectedMimeType."
              "BlockedWithoutSniffing.HasNoSniff",
              has_nosniff_header_);
        } else if (protection_decision ==
                   CrossOriginProtectionDecision::kBlockedAfterSniffing) {
          UMA_HISTOGRAM_BOOLEAN(
              "SiteIsolation.CORBProtection.CORSHeuristic.ProtectedMimeType."
              "BlockedAfterSniffingWithRangeSupport",
              supports_range_requests_);
        }
        break;
      case kPublic:
        UMA_HISTOGRAM_ENUMERATION(
            "SiteIsolation.CORBProtection.CORSHeuristic.PublicMimeType",
            protection_decision);
        break;
      case kOther:
        UMA_HISTOGRAM_ENUMERATION(
            "SiteIsolation.CORBProtection.CORSHeuristic.OtherMimeType",
            protection_decision);
    }
  }
  if (seems_sensitive_from_cache_heuristic_) {
    switch (mime_type_bucket_) {
      case kProtected:
        UMA_HISTOGRAM_ENUMERATION(
            "SiteIsolation.CORBProtection.CacheHeuristic.ProtectedMimeType",
            protection_decision);
        if (protection_decision == CrossOriginProtectionDecision::kBlock) {
          UMA_HISTOGRAM_BOOLEAN(
              "SiteIsolation.CORBProtection.CacheHeuristic.ProtectedMimeType."
              "BlockedWithRangeSupport",
              supports_range_requests_);
          UMA_HISTOGRAM_BOOLEAN(
              "SiteIsolation.CORBProtection.CacheHeuristic.ProtectedMimeType."
              "BlockedWithoutSniffing.HasNoSniff",
              has_nosniff_header_);
        } else if (protection_decision ==
                   CrossOriginProtectionDecision::kBlockedAfterSniffing) {
          UMA_HISTOGRAM_BOOLEAN(
              "SiteIsolation.CORBProtection.CacheHeuristic.ProtectedMimeType."
              "BlockedAfterSniffingWithRangeSupport",
              supports_range_requests_);
        }
        break;
      case kPublic:
        UMA_HISTOGRAM_ENUMERATION(
            "SiteIsolation.CORBProtection.CacheHeuristic.PublicMimeType",
            protection_decision);
        break;
      case kOther:
        UMA_HISTOGRAM_ENUMERATION(
            "SiteIsolation.CORBProtection.CacheHeuristic.OtherMimeType",
            protection_decision);
    }
  }
  // Also log if the server supports range requests, since these may allow
  // bypassing CORB.
  UMA_HISTOGRAM_BOOLEAN(
      "SiteIsolation.CORBProtection.SensitiveWithRangeSupport",
      supports_range_requests_);
}

}  // namespace network::corb
