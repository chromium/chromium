// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub mod cxx;
pub mod parser;

pub use parser::{decode_xml_bytes, decode_xml_str};
