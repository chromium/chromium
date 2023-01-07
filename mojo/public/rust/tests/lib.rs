// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[macro_use]
extern crate mojo;

#[macro_use]
mod util;

// These test suites work more or less. They still rely on some old Mojo
// behavior such as wait sets (see https://codereview.chromium.org/2744943002).
// These are temporarily replaced in support.cc.
mod integration;
mod run_loop;
mod system;

// Has one broken test that panics during a panic. It's disabled for now.
mod regression;

// These tests have two problems:
// * lots of test data from the Mojo SDK doesn't exist / has changed
// * the mojom encoding has changed since this code was written
//
// The code in crate/bindings/{encoding,decoding}.rs needs to be updated and
// the tests must be updated to use the available test data
//
mod encoding;
mod validation;
