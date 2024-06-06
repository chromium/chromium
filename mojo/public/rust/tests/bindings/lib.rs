// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Test Rust code generated for mojom test definitions, and associated code for
//! bindings support.

mod validation;

use rust_gtest_interop::prelude::*;

chromium::import! {
    "//mojo/public/interfaces/bindings/tests:test_mojom_import_rust" as test_mojom_import;
    "//mojo/public/interfaces/bindings/tests:test_interfaces_rust" as test_interfaces;
    "//mojo/public/rust:mojo_bindings" as bindings;
}

#[gtest(MojoBindingsTest, Basic)]
fn test() {
    use test_mojom_import::sample_import::*;

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

    use test_interfaces::rect::*;
    use test_interfaces::test_structs::*;

    let _rect_pair =
        RectPair { first: Some(Box::new(Rect { x: 1, y: 1, width: 1, height: 1 })), second: None };
}

#[gtest(MojoBindingsTest, Bytemuck)]
fn test() {
    let bytes: [u8; 16] = [16, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0];
    let point: test_mojom_import::sample_import::Point_Data = bytemuck::cast(bytes);

    expect_eq!(point._header.size, 16);
    expect_eq!(point._header.version, 0);
    expect_eq!(point.x, 1);
    expect_eq!(point.y, 2);
}
