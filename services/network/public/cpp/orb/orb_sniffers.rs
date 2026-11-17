// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//services/network/public/cpp:orb_sniffers_rs_api" as orb_sniffers;
}

use orb_sniffers::SniffingResult;

enum CompareCase {
    Sensitive,
    InsensitiveAscii,
}

fn strip_utf8_bom(data: &[u8]) -> &[u8] {
    // https://en.wikipedia.org/wiki/Byte_order_mark#UTF-8
    const UTF8_BOM: &[u8] = "\u{FEFF}".as_bytes();
    data.strip_prefix(UTF8_BOM).unwrap_or(data)
}

// Returns `SniffingResult::kYes` if `data` starts with one of the string
// patterns in `signatures`, `SniffingResult::kMaybe` if `data` is a prefix of
// one of the patterns in `signatures`, and `SniffingResult::kNo` otherwise.
fn matches_signature(
    data: &[u8],
    signatures: &[&[u8]],
    compare_case: CompareCase,
) -> SniffingResult {
    let eq = match compare_case {
        CompareCase::Sensitive => <[u8]>::eq,
        CompareCase::InsensitiveAscii => <[u8]>::eq_ignore_ascii_case,
    };
    for &signature in signatures {
        let len = data.len().min(signature.len());
        if eq(&data[..len], &signature[..len]) {
            return if data.len() >= signature.len() {
                SniffingResult::kYes
            } else {
                SniffingResult::kMaybe
            };
        }
    }
    SniffingResult::kNo
}

fn skip_until_javascript_line_terminator(mut hay: &[u8]) -> Option<&[u8]> {
    // https://www.ecma-international.org/ecma-262/8.0/index.html#prod-LineTerminator
    // defines LineTerminator ::= <LF> | <CR> | <LS> | <PS>.
    //
    // https://www.ecma-international.org/ecma-262/8.0/index.html#sec-line-terminators
    // defines <LF>, <CR>, <LS> ::= "\u2028", <PS> ::= "\u2029".
    //
    // In UTF8 encoding <LS> is 0xE2 0x80 0xA8 and <PS> is 0xE2 0x80 0xA9.
    const LS: &[u8] = "\u{2028}".as_bytes();
    const PS: &[u8] = "\u{2029}".as_bytes();
    loop {
        let offset = hay.iter().position(|&b| matches!(b, b'\n' | b'\r' | b'\xe2'))?;
        hay = &hay[offset..];

        // TODO(lukasza): Prevent matching 3 bytes that span/straddle 2 UTF8
        // characters.
        if hay[0] != b'\xe2' || hay.starts_with(LS) || hay.starts_with(PS) {
            return Some(hay);
        }

        hay = &hay[1..]; // Skip the \xe2 character.
    }
}

// Checks if `data` starts with an HTML comment (i.e. with "<!-- ... -->").
// - If there is a valid, terminated comment then returns `Ok` with the
//   remaining slice after the comment. Note that the SingleLineHTMLCloseComment
//   ECMAScript rule is taken into account, which means that characters
//   following an HTML comment are consumed up to the nearest line-terminating
//   character.
// - If there is a start of a comment, but the comment is not completed (e.g.
//   `data` == "<!-" or `data` == "<!-- not terminated yet") then returns
//   `Err(SniffingResult::kMaybe)`.
// - Returns `Err(SniffingResult::kNo)` otherwise.
fn skip_html_comment(data: &[u8]) -> Result<&[u8], SniffingResult> {
    const START_STRING: &[u8] = b"<!--";
    let Some(rest) = data.strip_prefix(START_STRING) else {
        return Err(if START_STRING.starts_with(data) {
            SniffingResult::kMaybe
        } else {
            SniffingResult::kNo
        });
    };

    const END_STRING: &[u8] = b"-->";
    let end_offset = rest
        .windows(END_STRING.len())
        .position(|w| w == END_STRING)
        .ok_or(SniffingResult::kMaybe)?;
    let after_comment = &rest[end_offset + END_STRING.len()..];

    // Skipping until the first line terminating character. See
    // https://crbug.com/839945 for the motivation behind this.
    skip_until_javascript_line_terminator(after_comment).ok_or(SniffingResult::kMaybe)
}

// This function is a slight modification of `net::SniffForHTML`.
pub fn sniff_for_html(data: &[u8]) -> SniffingResult {
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

    let mut data = strip_utf8_bom(data);
    while !data.is_empty() {
        // Based on https://infra.spec.whatwg.org/#ascii-whitespace
        data = data.trim_ascii_start();

        match matches_signature(data, HTML_SIGNATURES, CompareCase::InsensitiveAscii) {
            SniffingResult::kNo => {}
            result => return result,
        }

        data = match skip_html_comment(data) {
            Ok(rest) => rest,
            Err(result) => return result,
        };
    }

    // All of `data` was consumed, without a clear determination.
    SniffingResult::kMaybe
}

pub fn sniff_for_xml(data: &[u8]) -> SniffingResult {
    const XML_SIGNATURES: &[&[u8]] = &[b"<?xml"];
    let data = strip_utf8_bom(data).trim_ascii_start();
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
    enum State {
        Start,
        LeftBrace,
        LeftQuote,
        Escape,
        RightQuote,
    }
    let mut state = State::Start;

    for &c in strip_utf8_bom(data) {
        state = match (state, c) {
            // Whitespace is ignored (outside of string literals).
            (s @ (State::Start | State::LeftBrace | State::RightQuote), c)
                if c.is_ascii_whitespace() =>
            {
                s
            }
            (State::Start, b'{') => State::LeftBrace,
            (State::LeftBrace, b'"') => State::LeftQuote,
            (State::LeftQuote, b'"') => State::RightQuote,
            (State::LeftQuote, b'\\') => State::Escape,
            // Simplification: don't bother rejecting hex escapes.
            (State::LeftQuote | State::Escape, _) => State::LeftQuote,
            (State::RightQuote, b':') => return SniffingResult::kYes,
            (State::Start | State::LeftBrace | State::RightQuote, _) => {
                return SniffingResult::kNo;
            }
        };
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
    match matches_signature(data, SCRIPT_BREAKING_PREFIXES, CompareCase::Sensitive) {
        // A non-empty JSON object also effectively introduces a JS syntax
        // error.
        SniffingResult::kNo => sniff_for_json(data),
        result => result,
    }
}
