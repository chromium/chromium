// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::{error::Error, pin::Pin, result::Result};

use content_public_browsertest_support::ContentBrowserTest;
use rust_gtest_interop::prelude::*;

#[gtest(SmokeRustBrowserTest, Smoke)]
#[gtest_suite(ContentBrowserTest)]
fn test(_bt: Pin<&mut ContentBrowserTest>) -> Result<(), Box<dyn Error>> {
    // let mut server = bt.embedded_test_server_mut();
    // assert_true!(server.start());

    // GURL url = server.GetURL(kTestFile);

    // let shell = bt.shell();
    // expect_true!(NavigateToURL(shell, url));

    Ok(())
}
