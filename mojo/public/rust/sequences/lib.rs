// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This crate defines the utilities necessary to use sequenced task runners
//! from Rust code. In the future it will likely be expanded to cover other
//! types of task runner.
//!
//! # Examples
//!
//! ```
//! let ptr: SequencedTaskRunnerHandle = SequencedTaskRunnerHandle::get_current_default().unwrap();
//! let do_something = || println!("I did something!");
//! ptr.post_task(do_something);
//! ```

pub mod callback;
pub mod cxx;
pub mod run_loop;
pub mod scoped_refptr;
pub mod sequenced_task_runner;

pub use crate::scoped_refptr::{CxxRefCounted, ScopedRefPtr};
pub use crate::sequenced_task_runner::SequencedTaskRunnerHandle;
