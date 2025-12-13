// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_LIBFUZZER_TESTS_FUZZ_TARGET_H_
#define TESTING_LIBFUZZER_TESTS_FUZZ_TARGET_H_

#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"

namespace fuzzing {

// Overengineered, but makes for readable `Fuzz()` callsites.
struct FuzzOptions {
  // How long to fuzz at most, in seconds.
  int timeout_secs = 0;
};

// Represents a fuzz target binary and the ability to fuzz with it.
//
// Thread-compatible.
class FuzzTarget final {
 public:
  // Builds a new fuzz target object for the given fuzzer binary.
  // Returns nullopt in case of error.
  static std::optional<FuzzTarget> Make(std::string_view fuzzer_name);

  // Instances are move-only.
  FuzzTarget(FuzzTarget&&) = default;
  FuzzTarget& operator=(FuzzTarget&&) = default;

  // Runs this fuzz target, and returns whether it exited successfully.
  //
  // Note that whether the target exited successfully or not depends on how it
  // was run, not only whether it found in crash. In out-of-process fuzzing
  // mode, this may be true even if the target found a crash.
  bool Fuzz(const FuzzOptions& options);

  // The last output (stdout and stderr) of running the target with `Fuzz()`.
  std::string_view output() const { return output_; }

  // Returns all the crashing inputs found by this fuzz target across all calls
  // to `Fuzz()`.
  std::vector<std::string> GetCrashingInputs() const;

 private:
  FuzzTarget(std::string_view fuzzer_name);

  // It would be neater to keep the libfuzzer/centipede details hidden in the
  // .cc file instead, but this:
  //
  // a) avoids unused code warnings
  // b) helps detect most compile errors in all build configurations
  // c) makes for simpler function signatures

  // The command line to execute for `Fuzz()`.
  base::CommandLine FuzzCommandLine(const FuzzOptions& options) const;
  base::CommandLine LibfuzzerCommandLine(const FuzzOptions& options) const;
  base::CommandLine CentipedeCommandLine(const FuzzOptions& options) const;

  // The directory in which crashing inputs are stored.
  base::FilePath CrashingInputsDir() const;
  base::FilePath LibfuzzerCrashingInputsDir() const;
  base::FilePath CentipedeCrashingInputsDir() const;

  // Path to the fuzz target binary.
  base::FilePath fuzz_target_path_;

  // Temp directory in which to store working files and crashing inputs.
  base::ScopedTempDir temp_dir_;

  // See `output()`.
  std::string output_;
};

}  // namespace fuzzing

#endif  // TESTING_LIBFUZZER_TESTS_FUZZ_TARGET_H_
