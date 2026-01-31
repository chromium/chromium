// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub mod data_pipe;
pub mod mojo_types;
// FOR_RELEASE: Remove the raw_trap dependencies so that only `trap` is part of
// this library's public interface.
pub mod message_pipe;
pub mod raw_trap;
pub mod trap;
