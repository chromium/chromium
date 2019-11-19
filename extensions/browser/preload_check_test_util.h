// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PRELOAD_CHECK_TEST_UTIL_H_
#define EXTENSIONS_BROWSER_PRELOAD_CHECK_TEST_UTIL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "extensions/browser/preload_check.h"

namespace base {
class RunLoop;
}

namespace extensions {

// Provides a callback method for a PreloadCheck that stores its results.
class PreloadCheckRunner {
 public:
  PreloadCheckRunner();
  virtual ~PreloadCheckRunner();

  // Starts the check, providing OnCheckComplete as the callback.
  void Run(PreloadCheck* check);

  // Starts the check and waits for its callback to execute.
  void RunUntilComplete(PreloadCheck* check);

  // Runs the message loop until OnCheckComplete is called.
  void WaitForComplete();

  // Runs the message loop until idle. Useful to see whether OnCheckComplete is
  // called without waiting indefinitely.
  void WaitForIdle();

  PreloadCheck::ResultCallback GetCallback();

  const PreloadCheck::Errors& errors() const { return errors_; }
  bool called() const { return called_; }

 private:
  void OnCheckComplete(const PreloadCheck::Errors& errors);

  PreloadCheck::Errors errors_;
  bool called_;

  // Using a RunLoop data member would trigger tricky timing troubles.
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(PreloadCheckRunner);
};

// Stub for a PreloadCheck that calls the callback with the given error(s).
class PreloadCheckStub : public PreloadCheck {
 public:
  explicit PreloadCheckStub(const Errors& errors);
  ~PreloadCheckStub() override;

  void set_is_async(bool is_async) { is_async_ = is_async; }
  bool started() const { return started_; }

  // PreloadCheck:
  void Start(ResultCallback callback) override;

 private:
  void RunCallback(ResultCallback callback);

  bool is_async_ = false;
  bool started_ = false;
  Errors errors_;

  base::WeakPtrFactory<PreloadCheckStub> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PreloadCheckStub);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PRELOAD_CHECK_TEST_UTIL_H_
