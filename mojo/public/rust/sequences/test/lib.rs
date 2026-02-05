// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//mojo/public/rust/sequences";
    "//mojo/public/rust/sequences:test_cxx";
}

use rust_gtest_interop::prelude::*;
use sequences::{ScopedRefPtr, SequencedTaskRunnerHandle};
use std::sync::{Arc, RwLock};

fn get_test_ref_ptr(b: *mut bool) -> ScopedRefPtr<test_cxx::ffi::TestRefCounted> {
    // SAFETY: CreateTestRefCounted provides a released pointer
    unsafe { ScopedRefPtr::wrap_ref_counted(test_cxx::ffi::CreateTestRefCounted(&mut *b)) }.unwrap()
}

#[gtest(RustSequences, TestScopedRefPtr)]
fn test_scoped_refptr() {
    // Put the flag in an UnsafeCell so Rust won't move it around while C++
    // has it, and the borrow checker won't stop us from examining it.
    let destroyed_flag: std::cell::UnsafeCell<bool> = false.into();
    let test_ref_counted_ptr = get_test_ref_ptr(destroyed_flag.get());
    expect_true!(test_ref_counted_ptr.HasOneRef());
    expect_true!(test_ref_counted_ptr.HasAtLeastOneRef());
    // SAFETY: We're single-threaded so nothing else is examining this bool
    expect_false!(unsafe { *destroyed_flag.get() });

    let test_ref_counted_ptr2 = test_ref_counted_ptr.clone();
    expect_false!(test_ref_counted_ptr.HasOneRef());
    expect_false!(test_ref_counted_ptr2.HasOneRef());
    expect_true!(test_ref_counted_ptr.HasAtLeastOneRef());
    expect_true!(test_ref_counted_ptr2.HasAtLeastOneRef());
    // SAFETY: We're single-threaded so nothing else is examining this bool
    expect_false!(unsafe { *destroyed_flag.get() });

    drop(test_ref_counted_ptr);
    expect_true!(test_ref_counted_ptr2.HasOneRef());
    expect_true!(test_ref_counted_ptr2.HasAtLeastOneRef());
    // SAFETY: We're single-threaded so nothing else is examining this bool
    expect_false!(unsafe { *destroyed_flag.get() });

    drop(test_ref_counted_ptr2);
    // SAFETY: No other references to this bool exist anymore
    expect_true!(unsafe { *destroyed_flag.get() });
}

#[gtest(RustSequences, TestSequencedTaskrunner)]
fn test_sequenced_task_runner() {
    // Set up the environment so we can get a task runner and execute the tasks
    let _task_env = test_cxx::ffi::CreateTaskEnvironment();

    let task_runner = SequencedTaskRunnerHandle::get_current_default()
        .expect("We just initialized an environment so there should be a default task runner");

    let run_tasks = SequencedTaskRunnerHandle::run_all_current_tasks_on_default_runner_for_testing;

    // FOR_RELEASE: It would be nice to make our own type that wraps Arc<RwLock>
    // to provide a nicer API to users (in particular, no blocking functions).
    let count: Arc<RwLock<i32>> = Arc::new(RwLock::new(0));

    let count_clone = count.clone();
    let add_1 = move || {
        // `try_write` will never block. It should always succeed because the
        // data is accessed via the sequence.
        let mut count_value = count_clone.try_write().unwrap();
        *count_value += 1;
    };
    let count_clone = count.clone();
    let times_2 = move || {
        let mut count_value = count_clone.try_write().unwrap();
        *count_value *= 2;
    };
    let count_clone = count.clone();
    let sub_3 = move || {
        let mut count_value = count_clone.try_write().unwrap();
        *count_value -= 3;
    };

    task_runner.post_task(add_1.clone());
    task_runner.post_task(times_2.clone());
    task_runner.post_task(times_2.clone());
    task_runner.post_task(times_2.clone());
    task_runner.post_task(sub_3.clone());
    task_runner.post_task(times_2.clone());

    expect_eq!(*count.try_read().unwrap(), 0);
    run_tasks();
    expect_eq!(*count.try_read().unwrap(), 10);

    let count_clone = count.clone();
    task_runner.post_task(move || {
        let mut count_value = count_clone.try_write().unwrap();
        *count_value *= 100;
    });

    run_tasks();
    expect_eq!(*count.try_read().unwrap(), 1000);
}
