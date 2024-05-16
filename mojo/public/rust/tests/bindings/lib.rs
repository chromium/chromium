// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Simply test that we can compile some generated Rust struct bindings and that
//! they have the expected fields.

use rust_gtest_interop::prelude::*;

chromium::import! {
    "//mojo/public/interfaces/bindings/tests:test_mojom_import_rust" as mojom;
}

#[gtest(MojoBindingsTest, Basic)]
fn test() {
    use mojom::sample_import::*;

    expect_eq!(Shape::RECTANGLE.0, 1);
    expect_eq!(Shape::CIRCLE.0, 2);
    expect_eq!(Shape::TRIANGLE.0, 3);
    expect_eq!(Shape::LAST, Shape::TRIANGLE);

    expect_eq!(AnotherShape::RECTANGLE.0, 10);
    expect_eq!(AnotherShape::CIRCLE.0, 11);
    expect_eq!(AnotherShape::TRIANGLE.0, 12);

    expect_eq!(YetAnotherShape::RECTANGLE.0, 20);
    expect_eq!(YetAnotherShape::CIRCLE.0, 21);
    expect_eq!(YetAnotherShape::TRIANGLE.0, 22);

    let _point = Point { x: 1i32, y: 1i32 };
}
