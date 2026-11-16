// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//services/network/public/cpp:orb_sniffers_rs_api" as orb_sniffers;
}

use orb_sniffers::SniffingResult;

#[derive(Clone, Copy)]
enum CompareCase {
    Sensitive,
    InsensitiveAscii,
}

fn starts_with(str: &[u8], search_for: &[u8], compare_case: CompareCase) -> bool {
    let Some(prefix) = str.get(..search_for.len()) else {
        return false;
    };
    match compare_case {
        CompareCase::Sensitive => prefix == search_for,
        CompareCase::InsensitiveAscii => prefix.eq_ignore_ascii_case(search_for),
    }
}

fn strip_utf8_bom(data: &[u8]) -> &[u8] {
    // https://en.wikipedia.org/wiki/Byte_order_mark#UTF-8
    const UTF8_BOM: &[u8] = b"\xEF\xBB\xBF";
    data.strip_prefix(UTF8_BOM).unwrap_or(data)
}

// Based on https://infra.spec.whatwg.org/#ascii-whitespace
const WHITESPACE_CHARS: &[u8] = b"\t\n\x0c\r ";

fn trim_leading_whitespace(data: &[u8]) -> &[u8] {
    match data.iter().position(|c| !WHITESPACE_CHARS.contains(c)) {
        None => &[],
        Some(offset) => &data[offset..],
    }
}

// Returns `SniffingResult::kYes` if `data` starts with one of the string
// patterns in `signatures`, `SniffingResult::kMaybe` if `data` is a prefix of
// one of the patterns in `signatures`, and `SniffingResult::kNo` otherwise.
fn matches_signature(
    data: &[u8],
    signatures: &[&[u8]],
    compare_case: CompareCase,
) -> SniffingResult {
    for &signature in signatures {
        if signature.len() <= data.len() {
            if starts_with(data, signature, compare_case) {
                // When `signature` is a prefix of `data`, it constitutes a
                // match.
                return SniffingResult::kYes;
            }
        } else if starts_with(signature, data, compare_case) {
            // When `data` is a prefix of `signature`, that means that
            // subsequent bytes in the stream could cause a match to occur.
            return SniffingResult::kMaybe;
        }
    }
    SniffingResult::kNo
}

fn find_first_javascript_line_terminator(hay: &[u8], mut pos: usize) -> Option<usize> {
    // https://www.ecma-international.org/ecma-262/8.0/index.html#prod-LineTerminator
    // defines LineTerminator ::= <LF> | <CR> | <LS> | <PS>.
    //
    // https://www.ecma-international.org/ecma-262/8.0/index.html#sec-line-terminators
    // defines <LF>, <CR>, <LS> ::= "\u2028", <PS> ::= "\u2029".
    //
    // In UTF8 encoding <LS> is 0xE2 0x80 0xA8 and <PS> is 0xE2 0x80 0xA9.
    loop {
        let offset = hay[pos..].iter().position(|&b| matches!(b, b'\n' | b'\r' | b'\xe2'))?;
        pos += offset;

        if hay[pos] != b'\xe2' {
            debug_assert!(hay[pos] == b'\r' || hay[pos] == b'\n');
            return Some(pos);
        }

        // TODO(lukasza): Prevent matching 3 bytes that span/straddle 2 UTF8
        // characters.
        let substr = &hay[pos..];
        if substr.starts_with("\u{2028}".as_bytes()) || substr.starts_with("\u{2029}".as_bytes()) {
            return Some(pos);
        }

        pos += 1; // Skip the \xe2 character.
    }
}

// Checks if `data` starts with an HTML comment (i.e. with "<!-- ... -->").
// - If there is a valid, terminated comment then returns
//   `SniffingResult::kYes`.
// - If there is a start of a comment, but the comment is not completed (e.g.
//   `data` == "<!-" or `data` == "<!-- not terminated yet") then returns
//   `SniffingResult::kMaybe`.
// - Returns `SniffingResult::kNo` otherwise.
//
// Mutates `data` to advance past the comment when returning
// `SniffingResult::kYes`. Note that SingleLineHTMLCloseComment ECMAScript rule
// is taken into account which means that characters following an HTML comment
// are consumed up to the nearest line terminating character.
fn maybe_skip_html_comment(data: &mut &[u8]) -> SniffingResult {
    const START_STRING: &[u8] = b"<!--";
    if !data.starts_with(START_STRING) {
        if START_STRING.starts_with(data) {
            return SniffingResult::kMaybe;
        }
        return SniffingResult::kNo;
    }

    const END_STRING: &[u8] = b"-->";
    let Some(end_offset) =
        data[START_STRING.len()..].windows(END_STRING.len()).position(|w| w == END_STRING)
    else {
        return SniffingResult::kMaybe;
    };
    let end_of_html_comment = START_STRING.len() + end_offset + END_STRING.len();

    // Skipping until the first line terminating character. See
    // https://crbug.com/839945 for the motivation behind this.
    let Some(end_of_line) = find_first_javascript_line_terminator(data, end_of_html_comment) else {
        return SniffingResult::kMaybe;
    };

    // Found real end of the combined HTML/JS comment.
    *data = &data[end_of_line..];
    SniffingResult::kYes
}

// This function is a slight modification of `net::SniffForHTML`.
pub fn sniff_for_html(mut data: &[u8]) -> SniffingResult {
    // The content sniffers used by Chrome and Firefox are using "<!--" as one
    // of the HTML signatures, but it also appears in valid JavaScript,
    // considered as well-formed JS by the browser. Since we do not want to
    // block any JS, we exclude it from our HTML signatures. This can weaken our
    // ORB policy, but we can break fewer websites.
    //
    // Note that <body> and <br> are not included below, since <b is a prefix of
    // them.
    //
    // TODO(dsjang): parameterize `net::SniffForHTML` with an option that
    // decides whether to include <!-- or not, so that we can remove this
    // function.
    const HTML_SIGNATURES: &[&[u8]] = &[
        b"<!doctype html", // HTML5 spec
        b"<script",        // HTML5 spec, Mozilla
        b"<html",          // HTML5 spec, Mozilla
        b"<head",          // HTML5 spec, Mozilla
        b"<iframe",        // Mozilla
        b"<h1",            // Mozilla
        b"<div",           // Mozilla
        b"<font",          // Mozilla
        b"<table",         // Mozilla
        b"<a",             // Mozilla
        b"<style",         // Mozilla
        b"<title",         // Mozilla
        b"<b",             // Mozilla (note: subsumes <body>, <br>)
        b"<p",             // Mozilla
    ];

    data = strip_utf8_bom(data);
    while !data.is_empty() {
        data = trim_leading_whitespace(data);

        let signature_match =
            matches_signature(data, HTML_SIGNATURES, CompareCase::InsensitiveAscii);
        if signature_match != SniffingResult::kNo {
            return signature_match;
        }

        let comment_match = maybe_skip_html_comment(&mut data);
        if comment_match != SniffingResult::kYes {
            return comment_match;
        }
    }

    // All of `data` was consumed, without a clear determination.
    SniffingResult::kMaybe
}

pub fn sniff_for_xml(data: &[u8]) -> SniffingResult {
    let data = trim_leading_whitespace(strip_utf8_bom(data));
    const XML_SIGNATURES: &[&[u8]] = &[b"<?xml"];
    matches_signature(data, XML_SIGNATURES, CompareCase::Sensitive)
}

pub fn sniff_for_json(data: &[u8]) -> SniffingResult {
    // Currently this function looks for an opening brace ('{'), followed by a
    // double-quoted string literal, followed by a colon. Importantly, such a
    // sequence is a Javascript syntax error: although the JSON object syntax is
    // exactly Javascript's object-initializer syntax, a Javascript object-
    // initializer expression is not valid as a standalone Javascript statement.
    //
    // TODO(nick): We have to come up with a better way to sniff JSON. The
    // following are known limitations of this function:
    // https://crbug.com/795470/ Support non-dictionary values (e.g. lists)
    #[derive(PartialEq, Eq)]
    enum State {
        Start,
        LeftBrace,
        LeftQuote,
        Escape,
        RightQuote,
    }
    let mut state = State::Start;

    let data = strip_utf8_bom(data);
    for &c in data {
        if state != State::LeftQuote && state != State::Escape {
            // Whitespace is ignored (outside of string literals)
            if WHITESPACE_CHARS.contains(&c) {
                continue;
            }
        }

        match state {
            State::Start => {
                if c == b'{' {
                    state = State::LeftBrace;
                } else {
                    return SniffingResult::kNo;
                }
            }
            State::LeftBrace => {
                if c == b'"' {
                    state = State::LeftQuote;
                } else {
                    return SniffingResult::kNo;
                }
            }
            State::LeftQuote => {
                if c == b'"' {
                    state = State::RightQuote;
                } else if c == b'\\' {
                    state = State::Escape;
                }
            }
            State::Escape => {
                // Simplification: don't bother rejecting hex escapes.
                state = State::LeftQuote;
            }
            State::RightQuote => {
                if c == b':' {
                    return SniffingResult::kYes;
                }
                return SniffingResult::kNo;
            }
        }
    }
    SniffingResult::kMaybe
}

pub fn sniff_for_fetch_only_resource(data: &[u8]) -> SniffingResult {
    // `SCRIPT_BREAKING_PREFIXES` contains prefixes that are conventionally used
    // to prevent a JSON response from becoming a valid Javascript program (an
    // attack vector known as XSSI). The presence of such a prefix is a strong
    // signal that the resource is meant to be consumed only by the fetch API or
    // XMLHttpRequest, and is meant to be protected from use in non-CORS, cross-
    // origin contexts like <script>, <img>, etc.
    //
    // These prefixes work either by inducing a syntax error, or inducing an
    // infinite loop. In either case, the prefix must create a guarantee that no
    // matter what bytes follow it, the entire response would be worthless to
    // execute as a <script>.
    const SCRIPT_BREAKING_PREFIXES: &[&[u8]] = &[
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
        b")]}'",
        // Apache struts: https://struts.apache.org/plugins/json/#prefix
        b"{}&&",
        // Spring framework (historically): https://goo.gl/JYPFAv
        b"{} &&",
        // Infinite loops.
        b"for(;;);", // observed on facebook.com
        b"while(1);",
        b"for (;;);",
        b"while (1);",
    ];

    let data = strip_utf8_bom(data);
    let has_parser_breaker =
        matches_signature(data, SCRIPT_BREAKING_PREFIXES, CompareCase::Sensitive);
    if has_parser_breaker != SniffingResult::kNo {
        return has_parser_breaker;
    }

    // A non-empty JSON object also effectively introduces a JS syntax error.
    sniff_for_json(data)
}
