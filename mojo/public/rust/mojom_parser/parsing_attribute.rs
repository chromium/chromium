// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! FOR_RELEASE: Docs

#[proc_macro_derive(MojomParse)]
pub fn derive(_input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    // FOR_RELEASE: Do this
    proc_macro::TokenStream::new()
}
