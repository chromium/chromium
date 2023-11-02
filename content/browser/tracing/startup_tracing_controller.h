// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_STARTUP_TRACING_CONTROLLER_H_
#define CONTENT_BROWSER_TRACING_STARTUP_TRACING_CONTROLLER_H_

#include "base/files/file_path.h"
#include "base/threading/sequence_bound.h"
#include "content/common/content_export.h"

namespace content {

// Class responsible for starting and stopping startup tracing as configured by
// StartupTracingConfig. All interactions with it are limited to UI thread, but
// the actual logic lives on a background ThreadPool sequence.
class CONTENT_EXPORT StartupTracingController {
 public:
  StartupTracingController();
  ~StartupTracingController();

  static StartupTracingController& GetInstance();

  // Stop the trace recording, write the trace to disk and block until complete.
  // Intended to be used in the situation when the browser process is going to
  // crash (e.g. DCHECK failure) and we want to avoid losing the trace data. Can
  // be called from any thread.
  // May not succeed if called from a sequence that is required to be responsive
  // during trace finalisation.
  static void EmergencyStop();

  void StartIfNeeded();
  void WaitUntilStopped();
  void ShutdownAndWaitForStopIfNeeded();

  // By default, a trace is written into a temporary file which then is renamed,
  // however this can lead to data loss when the browser process crashes.
  // Embedders can disable this (especially if a name provided to
  // SetDefaultBasename makes it clear that the trace is incomplete and final
  // name will be provided via SetDefaultBasename call before calling Stop).
  enum class TempFilePolicy {
    kUseTemporaryFile,
    kWriteDirectly,
  };
  void SetUsingTemporaryFile(TempFilePolicy temp_file_policy);

  // Set default basename for the trace output file to allow //content embedders
  // to customise it using some metadata (like test names).
  //
  // If --enable-trace-output is a directory (default value, empty, designated
  // "current directory"), then the startup trace will be written in a file with
  // the given basename in this directory. Depending on the |extension_type|,
  // an appropriate extension (.json or .proto) will be added.
  //
  // Note that embedders can call it even after tracing has started and Perfetto
  // started streaming the trace into it — in that case,
  // StartupTracingController will rename the file after finishing. However,
  // this is guaranteed to work only when tracing lasts until Stop() (not with
  // duration-based tracing).
  enum class ExtensionType {
    kAppendAppropriate,
    kNone,
  };
  void SetDefaultBasename(std::string basename, ExtensionType extension_type);
  // As the test harness calls SetDefaultBasename, expose ForTest() version for
  // the tests checking the StartupTracingController logic itself.
  void SetDefaultBasenameForTest(std::string basename,
                                 ExtensionType extension_type);

  bool is_finished_for_testing() const { return state_ == State::kStopped; }

  void set_continue_on_shutdown_for_testing() {
    should_continue_on_shutdown_ = true;
  }

 private:
  void Stop(base::OnceClosure on_finished_callback);

  void OnStoppedOnUIThread();

  base::FilePath GetOutputPath();

  enum class State {
    kNotEnabled,
    kRunning,
    kStopped,
  };
  State state_ = State::kNotEnabled;

  // All actual interactions with the tracing service and the process of writing
  // files happens on a background thread.
  class BackgroundTracer;
  base::SequenceBound<BackgroundTracer> background_tracer_;

  base::OnceClosure on_tracing_finished_;
  base::FilePath output_file_;

  std::string default_basename_;
  bool basename_for_test_set_ = false;
  // Used for testing only
  bool should_continue_on_shutdown_ = false;

  TempFilePolicy temp_file_policy_ = TempFilePolicy::kUseTemporaryFile;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_STARTUP_TRACING_CONTROLLER_H_
