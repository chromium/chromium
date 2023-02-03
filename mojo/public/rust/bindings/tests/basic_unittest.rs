// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Simply test that we can compile some generated Rust struct bindings and that
//! they have the expected fields.

// TODO(crbug.com/1274864): find a better way to reference generated bindings.
// This is not sustainable, but it's sufficient for a single test while
// prototyping.
mod rect_mojom {
    include!(concat!(env!("GEN_DIR"), "/mojo/public/interfaces/bindings/tests/rect.mojom.rs"));
}

#[test]
fn basic_struct_test() {
    let _rect = rect_mojom::Rect { x: 1, y: 1, width: 1, height: 1 };
}
