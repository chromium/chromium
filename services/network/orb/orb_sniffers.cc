// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/orb/orb_sniffers.h"

#include <stddef.h>

#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "net/base/mime_sniffer.h"

namespace network::orb {

namespace {

void AdvancePastWhitespace(std::string_view* data) {
  size_t offset = data->find_first_not_of(" \t\r\n");
  if (offset == std::string_view::npos) {
    // |data| was entirely whitespace.
    *data = std::string_view();
  } else {
    data->remove_prefix(offset);
  }
}

// Returns kYes if |data| starts with one of the string patterns in
// |signatures|, kMaybe if |data| is a prefix of one of the patterns in
// |signatures|, and kNo otherwise.
//
// When kYes is returned, the matching prefix is erased from |data|.
SniffingResult MatchesSignature(std::string_view* data,
                                const std::string_view signatures[],
                                size_t arr_size,
                                base::CompareCase compare_case) {
  for (size_t i = 0; i < arr_size; ++i) {
    if (signatures[i].length() <= data->length()) {
      if (base::StartsWith(*data, signatures[i], compare_case)) {
        // When |signatures[i]| is a prefix of |data|, it constitutes a match.
        // Strip the matching characters, and return.
        data->remove_prefix(signatures[i].length());
        return kYes;
      }
    } else {
      if (base::StartsWith(signatures[i], *data, compare_case)) {
        // When |data| is a prefix of |signatures[i]|, that means that
        // subsequent bytes in the stream could cause a match to occur.
        return kMaybe;
      }
    }
  }
  return kNo;
}

size_t FindFirstJavascriptLineTerminator(std::string_view hay, size_t pos) {
  // https://www.ecma-international.org/ecma-262/8.0/index.html#prod-LineTerminator
  // defines LineTerminator ::= <LF> | <CR> | <LS> | <PS>.
  //
  // https://www.ecma-international.org/ecma-262/8.0/index.html#sec-line-terminators
  // defines <LF>, <CR>, <LS> ::= "\u2028", <PS> ::= "\u2029".
  //
  // In UTF8 encoding <LS> is 0xE2 0x80 0xA8 and <PS> is 0xE2 0x80 0xA9.
  while (true) {
    pos = hay.find_first_of("\n\r\xe2", pos);
    if (pos == std::string_view::npos) {
      break;
    }

    if (hay[pos] != '\xe2') {
      DCHECK(hay[pos] == '\r' || hay[pos] == '\n');
      break;
    }

    // TODO(lukasza): Prevent matching 3 bytes that span/straddle 2 UTF8
    // characters.
    std::string_view substr = hay.substr(pos);
    if (base::StartsWith(substr, "\u2028") ||
        base::StartsWith(substr, "\u2029")) {
      break;
    }

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
SniffingResult MaybeSkipHtmlComment(std::string_view* data) {
  constexpr std::string_view kStartString = "<!--";
  if (!base::StartsWith(*data, kStartString)) {
    if (base::StartsWith(kStartString, *data)) {
      return kMaybe;
    }
    return kNo;
  }

  constexpr std::string_view kEndString = "-->";
  size_t end_of_html_comment = data->find(kEndString, kStartString.length());
  if (end_of_html_comment == std::string_view::npos) {
    return kMaybe;
  }
  end_of_html_comment += kEndString.length();

  // Skipping until the first line terminating character.  See
  // https://crbug.com/839945 for the motivation behind this.
  size_t end_of_line =
      FindFirstJavascriptLineTerminator(*data, end_of_html_comment);
  if (end_of_line == std::string_view::npos) {
    return kMaybe;
  }

  // Found real end of the combined HTML/JS comment.
  data->remove_prefix(end_of_line);
  return kYes;
}

}  // anonymous namespace

// This function is a slight modification of |net::SniffForHTML|.
SniffingResult SniffForHTML(std::string_view data) {
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
  static constexpr std::string_view kHtmlSignatures[] = {
      std::string_view("<!doctype html"),  // HTML5 spec
      std::string_view("<script"),         // HTML5 spec, Mozilla
      std::string_view("<html"),           // HTML5 spec, Mozilla
      std::string_view("<head"),           // HTML5 spec, Mozilla
      std::string_view("<iframe"),         // Mozilla
      std::string_view("<h1"),             // Mozilla
      std::string_view("<div"),            // Mozilla
      std::string_view("<font"),           // Mozilla
      std::string_view("<table"),          // Mozilla
      std::string_view("<a"),              // Mozilla
      std::string_view("<style"),          // Mozilla
      std::string_view("<title"),          // Mozilla
      std::string_view("<b"),  // Mozilla (note: subsumes <body>, <br>)
      std::string_view("<p")   // Mozilla
  };

  while (data.length() > 0) {
    AdvancePastWhitespace(&data);

    SniffingResult signature_match =
        MatchesSignature(&data, kHtmlSignatures, std::size(kHtmlSignatures),
                         base::CompareCase::INSENSITIVE_ASCII);
    if (signature_match != kNo) {
      return signature_match;
    }

    SniffingResult comment_match = MaybeSkipHtmlComment(&data);
    if (comment_match != kYes) {
      return comment_match;
    }
  }

  // All of |data| was consumed, without a clear determination.
  return kMaybe;
}

SniffingResult SniffForXML(std::string_view data) {
  // TODO(dsjang): Once CrossOriginReadBlocking is moved into the browser
  // process, we should do single-thread checking here for the static
  // initializer.
  AdvancePastWhitespace(&data);
  static constexpr std::string_view kXmlSignatures[] = {
      std::string_view("<?xml")};
  return MatchesSignature(&data, kXmlSignatures, std::size(kXmlSignatures),
                          base::CompareCase::SENSITIVE);
}

SniffingResult SniffForJSON(std::string_view data) {
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
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        continue;
      }
    }

    switch (state) {
      case kStartState:
        if (c == '{') {
          state = kLeftBraceState;
        } else {
          return kNo;
        }
        break;
      case kLeftBraceState:
        if (c == '"') {
          state = kLeftQuoteState;
        } else {
          return kNo;
        }
        break;
      case kLeftQuoteState:
        if (c == '"') {
          state = kRightQuoteState;
        } else if (c == '\\') {
          state = kEscapeState;
        }
        break;
      case kEscapeState:
        // Simplification: don't bother rejecting hex escapes.
        state = kLeftQuoteState;
        break;
      case kRightQuoteState:
        if (c == ':') {
          return kYes;
        }
        return kNo;
    }
  }
  return kMaybe;
}

SniffingResult SniffForFetchOnlyResource(std::string_view data) {
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
  static constexpr std::string_view kScriptBreakingPrefixes[] = {
      // Parser breaker prefix.
      //
      // Built into angular.js (followed by a comma and a newline):
      //   https://docs.angularjs.org/api/ng/service/$http
      //
      // Built into the Java Spring framework (followed by a comma and a space):
      //   https://goo.gl/xP7FWn
      //
      // Observed on google.com (without a comma, followed by a newline).
      std::string_view(")]}'"),

      // Apache struts: https://struts.apache.org/plugins/json/#prefix
      std::string_view("{}&&"),

      // Spring framework (historically): https://goo.gl/JYPFAv
      std::string_view("{} &&"),

      // Infinite loops.
      std::string_view("for(;;);"),  // observed on facebook.com
      std::string_view("while(1);"),
      std::string_view("for (;;);"),
      std::string_view("while (1);"),
  };
  SniffingResult has_parser_breaker = MatchesSignature(
      &data, kScriptBreakingPrefixes, std::size(kScriptBreakingPrefixes),
      base::CompareCase::SENSITIVE);
  if (has_parser_breaker != kNo) {
    return has_parser_breaker;
  }

  // A non-empty JSON object also effectively introduces a JS syntax error.
  return SniffForJSON(data);
}

}  // namespace network::orb
