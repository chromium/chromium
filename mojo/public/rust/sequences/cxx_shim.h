// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the C++ side of the cxx bindings defined in cxx.rs. See that file
// for documentation.

#ifndef MOJO_PUBLIC_RUST_SEQUENCES_CXX_SHIM_H_
#define MOJO_PUBLIC_RUST_SEQUENCES_CXX_SHIM_H_

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/rust/sequences/cxx.rs.h"

namespace rust_sequences {

// Return a raw pointer to the current default SequencedTaskRunner, since we
// can't pass scoped_refptr to rust directly.
base::SequencedTaskRunner* GetCurrentDefaultSequencedTaskRunnerForRust() {
  // Make a copy of the refptr to increment its count.
  scoped_refptr<base::SequencedTaskRunner> ptr =
      base::SequencedTaskRunner::GetCurrentDefault();
  // Convert the scoped_refptr to a raw pointer without decrementing its count.
  // This ensures the object won't get deleted before rust has a change to wrap
  // it.
  return ptr.release();
}

// Post a task to the given runner
bool PostTaskFromRust(base::SequencedTaskRunner& runner,
                      rust::Box<rust_sequences::RustOnceClosure> task) {
  // The location is almost completely useless, oh well.
  // FOR_RELASE: Figure out a rust location type later.
  return runner.PostTask(
      FROM_HERE,
      base::BindOnce(&rust_sequences::RustOnceClosure::run, std::move(task)));
}

// Create a new RunLoop on the heap so we can pass it over the cxx bridge
std::unique_ptr<base::RunLoop> CreateRunLoop() {
  return std::make_unique<base::RunLoop>();
}

// Run the run loop.
void RunRunLoop(const std::unique_ptr<base::RunLoop>& run_loop) {
  run_loop->Run();
}

// Quit the run loop.
void QuitRunLoop(const std::unique_ptr<base::RunLoop>& run_loop) {
  run_loop->Quit();
}

}  // namespace rust_sequences

#endif  // MOJO_PUBLIC_RUST_SEQUENCES_CXX_SHIM_H_
