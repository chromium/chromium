// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![deny(unsafe_code)]

use icu_properties::{
    props::{Graph, WhiteSpace},
    CodePointSetData, CodePointSetDataBorrowed,
};

const LEFT_TO_RIGHT_ISOLATE: u16 = 0x2066;
const RIGHT_TO_LEFT_ISOLATE: u16 = 0x2067;
const FIRST_STRONG_ISOLATE: u16 = 0x2068;
const POP_DIRECTIONAL_ISOLATE: u16 = 0x2069;
const HORIZONTAL_ELLIPSIS: u16 = 0x2026;
const MAX_STRING_LENGTH: usize = 64;

#[allow(unsafe_code)]
#[cxx::bridge(namespace = "device")]
pub mod ffi {
    extern "Rust" {
        fn has_graphic_character(bytes: &[u8]) -> bool;
        fn contain_string_for_display(string: &[u16]) -> Vec<u16>;
    }
}

const GRAPH_DATA: CodePointSetDataBorrowed<'static> = CodePointSetData::new::<Graph>();
const WHITESPACE_DATA: CodePointSetDataBorrowed<'static> = CodePointSetData::new::<WhiteSpace>();

fn is_isolate_initiator(c: u16) -> bool {
    c == LEFT_TO_RIGHT_ISOLATE || c == RIGHT_TO_LEFT_ISOLATE || c == FIRST_STRONG_ISOLATE
}

fn is_directional_isolate(c: u16) -> bool {
    is_isolate_initiator(c) || c == POP_DIRECTIONAL_ISOLATE
}

fn is_lead_surrogate(c: u16) -> bool {
    (0xD800..=0xDBFF).contains(&c)
}

pub fn has_graphic_character(bytes: &[u8]) -> bool {
    std::str::from_utf8(bytes).is_ok_and(|s| {
        s.chars().any(|c| {
            let u = c as u32;
            (u > 0xFFFF || !is_directional_isolate(u as u16)) && GRAPH_DATA.contains(c)
        })
    })
}

fn is_utf16_whitespace(c: u16) -> bool {
    c <= 0x001F || (0x007F..=0x009F).contains(&c) || WHITESPACE_DATA.contains32(c as u32)
}

fn collapse_whitespace(string: &[u16]) -> Vec<u16> {
    let mut result = Vec::with_capacity(string.len());
    let mut has_seen_word = false;
    let mut pending_whitespace = false;

    for &c in string {
        if is_utf16_whitespace(c) {
            if has_seen_word {
                pending_whitespace = true;
            }
        } else if is_directional_isolate(c) {
            result.push(c);
        } else {
            if pending_whitespace {
                result.push(0x0020);
                pending_whitespace = false;
            }
            result.push(c);
            has_seen_word = true;
        }
    }

    if !has_seen_word {
        return Vec::new();
    }
    result
}

fn truncate_string(string: &[u16], max_len: usize) -> Vec<u16> {
    if max_len == 0 {
        return Vec::new();
    }
    let non_isolate_count = string.iter().filter(|&&c| !is_directional_isolate(c)).count();
    if non_isolate_count <= max_len {
        return string.to_vec();
    }

    let target_non_isolate = max_len - 1;
    let mut non_isolate_seen = 0;
    let mut target_len = 0;
    if target_non_isolate > 0 {
        for (idx, &c) in string.iter().enumerate() {
            target_len = idx + 1;
            if !is_directional_isolate(c) {
                non_isolate_seen += 1;
                if non_isolate_seen == target_non_isolate {
                    break;
                }
            }
        }
    }

    // Strip trailing whitespace, directional isolates, and unpaired lead
    // surrogates before appending the ellipsis.
    while target_len > 0 {
        let last = string[target_len - 1];
        if is_utf16_whitespace(last) || is_directional_isolate(last) || is_lead_surrogate(last) {
            target_len -= 1;
        } else {
            break;
        }
    }

    let mut result = balance_directional_isolates(&string[..target_len]);
    result.push(HORIZONTAL_ELLIPSIS);
    result
}

fn balance_directional_isolates(string: &[u16]) -> Vec<u16> {
    let mut result = Vec::with_capacity(string.len());
    let mut depth: usize = 0;

    for &c in string {
        if is_isolate_initiator(c) {
            depth += 1;
        } else if c == POP_DIRECTIONAL_ISOLATE {
            if depth == 0 {
                continue; // Drop unmatched PDI
            }
            depth -= 1;
        }
        result.push(c);
    }

    result.resize(result.len() + depth, POP_DIRECTIONAL_ISOLATE);

    result
}

fn is_outer_isolate_wrapped(string: &[u16]) -> bool {
    if string.len() < 2
        || string.first() != Some(&FIRST_STRONG_ISOLATE)
        || string.last() != Some(&POP_DIRECTIONAL_ISOLATE)
    {
        return false;
    }
    let mut depth: usize = 1;
    for &c in &string[1..string.len() - 1] {
        if is_isolate_initiator(c) {
            depth += 1;
        } else if c == POP_DIRECTIONAL_ISOLATE {
            if depth == 1 {
                return false; // Outer FSI was closed early
            }
            depth -= 1;
        }
    }
    depth == 1
}

pub fn contain_string_for_display(mut string: &[u16]) -> Vec<u16> {
    if string.is_empty() {
        return Vec::new();
    }

    // Strip outer matching FSI and PDI if the string was already contained.
    if is_outer_isolate_wrapped(string) {
        string = &string[1..string.len() - 1];
    }

    let collapsed = collapse_whitespace(string);
    if collapsed.is_empty() {
        return Vec::new();
    }

    let truncated = truncate_string(&collapsed, MAX_STRING_LENGTH);

    let balanced = balance_directional_isolates(&truncated);
    if balanced.is_empty() {
        return Vec::new();
    }

    let mut result = Vec::with_capacity(balanced.len() + 2);
    result.push(FIRST_STRONG_ISOLATE);
    result.extend_from_slice(&balanced);
    result.push(POP_DIRECTIONAL_ISOLATE);
    result
}
