// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/orb/orb_sniffers.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "base/strings/whitespace_constants.h"

namespace network::orb {

namespace {

[[nodiscard]] std::string_view StripUtf8Bom(std::string_view data) {
  return base::RemovePrefix(data, base::kUtf8ByteOrderMark).value_or(data);
}

// Based on https://infra.spec.whatwg.org/#ascii-whitespace
constexpr std::string_view kWhitespaceChars = "\t\n\f\r ";

[[nodiscard]] std::string_view TrimLeadingWhitespace(std::string_view data) {
  return base::TrimString(data, kWhitespaceChars, base::TRIM_LEADING);
}

// Returns `kYes` if `data` starts with one of the string patterns in
// `signatures`, `kMaybe` if `data` is a prefix of one of the patterns in
// `signatures`, and `kNo` otherwise.
SniffingResult MatchesSignature(std::string_view data,
                                base::span<const std::string_view> signatures,
                                base::CompareCase compare_case) {
  for (std::string_view signature : signatures) {
    if (signature.length() <= data.length()) {
      if (base::StartsWith(data, signature, compare_case)) {
        // When `signature` is a prefix of `data`, it constitutes a match.
        return SniffingResult::kYes;
      }
    } else {
      if (base::StartsWith(signature, data, compare_case)) {
        // When `data` is a prefix of `signature`, that means that
        // subsequent bytes in the stream could cause a match to occur.
        return SniffingResult::kMaybe;
      }
    }
  }
  return SniffingResult::kNo;
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
    if (substr.starts_with("\u2028") || substr.starts_with("\u2029")) {
      break;
    }

    pos++;  // Skip the \xe2 character.
  }
  return pos;
}

// Checks if `data` starts with an HTML comment (i.e. with "<!-- ... -->").
// - If there is a valid, terminated comment then returns `kYes`.
// - If there is a start of a comment, but the comment is not completed (e.g.
//   `data` == "<!-" or `data` == "<!-- not terminated yet") then returns
//   `kMaybe`.
// - Returns `kNo` otherwise.
//
// Mutates `data` to advance past the comment when returning `kYes`. Note that
// SingleLineHTMLCloseComment ECMAScript rule is taken into account which means
// that characters following an HTML comment are consumed up to the nearest line
// terminating character.
SniffingResult MaybeSkipHtmlComment(std::string_view& data) {
  constexpr std::string_view kStartString = "<!--";
  if (!data.starts_with(kStartString)) {
    if (kStartString.starts_with(data)) {
      return SniffingResult::kMaybe;
    }
    return SniffingResult::kNo;
  }

  constexpr std::string_view kEndString = "-->";
  size_t end_of_html_comment = data.find(kEndString, kStartString.length());
  if (end_of_html_comment == std::string_view::npos) {
    return SniffingResult::kMaybe;
  }
  end_of_html_comment += kEndString.length();

  // Skipping until the first line terminating character. See
  // https://crbug.com/839945 for the motivation behind this.
  size_t end_of_line =
      FindFirstJavascriptLineTerminator(data, end_of_html_comment);
  if (end_of_line == std::string_view::npos) {
    return SniffingResult::kMaybe;
  }

  // Found real end of the combined HTML/JS comment.
  data.remove_prefix(end_of_line);
  return SniffingResult::kYes;
}

}  // namespace

// This function is a slight modification of `net::SniffForHTML`.
SniffingResult SniffForHTML(std::string_view data) {
  // The content sniffers used by Chrome and Firefox are using "<!--" as one of
  // the HTML signatures, but it also appears in valid JavaScript, considered as
  // well-formed JS by the browser. Since we do not want to block any JS, we
  // exclude it from our HTML signatures. This can weaken our ORB policy,
  // but we can break fewer websites.
  //
  // Note that <body> and <br> are not included below, since <b is a prefix of
  // them.
  //
  // TODO(dsjang): parameterize `net::SniffForHTML` with an option that decides
  // whether to include <!-- or not, so that we can remove this function.
  static constexpr auto kHtmlSignatures = std::to_array<std::string_view>({
      "<!doctype html",  // HTML5 spec
      "<script",         // HTML5 spec, Mozilla
      "<html",           // HTML5 spec, Mozilla
      "<head",           // HTML5 spec, Mozilla
      "<iframe",         // Mozilla
      "<h1",             // Mozilla
      "<div",            // Mozilla
      "<font",           // Mozilla
      "<table",          // Mozilla
      "<a",              // Mozilla
      "<style",          // Mozilla
      "<title",          // Mozilla
      "<b",              // Mozilla (note: subsumes <body>, <br>)
      "<p",              // Mozilla
  });

  data = StripUtf8Bom(data);
  while (!data.empty()) {
    data = TrimLeadingWhitespace(data);

    SniffingResult signature_match = MatchesSignature(
        data, kHtmlSignatures, base::CompareCase::INSENSITIVE_ASCII);
    if (signature_match != SniffingResult::kNo) {
      return signature_match;
    }

    SniffingResult comment_match = MaybeSkipHtmlComment(data);
    if (comment_match != SniffingResult::kYes) {
      return comment_match;
    }
  }

  // All of `data` was consumed, without a clear determination.
  return SniffingResult::kMaybe;
}

SniffingResult SniffForXML(std::string_view data) {
  data = StripUtf8Bom(data);
  data = TrimLeadingWhitespace(data);
  static constexpr auto kXmlSignatures =
      std::to_array<std::string_view>({"<?xml"});
  return MatchesSignature(data, kXmlSignatures, base::CompareCase::SENSITIVE);
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
  enum class State {
    kStart,
    kLeftBrace,
    kLeftQuote,
    kEscape,
    kRightQuote,
  } state = State::kStart;

  data = StripUtf8Bom(data);
  for (char c : data) {
    if (state != State::kLeftQuote && state != State::kEscape) {
      // Whitespace is ignored (outside of string literals)
      if (kWhitespaceChars.contains(c)) {
        continue;
      }
    }

    switch (state) {
      case State::kStart:
        if (c == '{') {
          state = State::kLeftBrace;
        } else {
          return SniffingResult::kNo;
        }
        break;
      case State::kLeftBrace:
        if (c == '"') {
          state = State::kLeftQuote;
        } else {
          return SniffingResult::kNo;
        }
        break;
      case State::kLeftQuote:
        if (c == '"') {
          state = State::kRightQuote;
        } else if (c == '\\') {
          state = State::kEscape;
        }
        break;
      case State::kEscape:
        // Simplification: don't bother rejecting hex escapes.
        state = State::kLeftQuote;
        break;
      case State::kRightQuote:
        if (c == ':') {
          return SniffingResult::kYes;
        }
        return SniffingResult::kNo;
    }
  }
  return SniffingResult::kMaybe;
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
  static constexpr auto kScriptBreakingPrefixes =
      std::to_array<std::string_view>({
          // Parser breaker prefix.
          //
          // Built into angular.js (followed by a comma and a newline):
          //   https://docs.angularjs.org/api/ng/service/$http
          //
          // Built into the Java Spring framework (followed by a comma and a
          // space):
          //   https://goo.gl/xP7FWn
          //
          // Observed on google.com (without a comma, followed by a newline).
          ")]}'",

          // Apache struts: https://struts.apache.org/plugins/json/#prefix
          "{}&&",

          // Spring framework (historically): https://goo.gl/JYPFAv
          "{} &&",

          // Infinite loops.
          "for(;;);",  // observed on facebook.com
          "while(1);",
          "for (;;);",
          "while (1);",
      });

  data = StripUtf8Bom(data);
  SniffingResult has_parser_breaker = MatchesSignature(
      data, kScriptBreakingPrefixes, base::CompareCase::SENSITIVE);
  if (has_parser_breaker != SniffingResult::kNo) {
    return has_parser_breaker;
  }

  // A non-empty JSON object also effectively introduces a JS syntax error.
  return SniffForJSON(data);
}

}  // namespace network::orb
