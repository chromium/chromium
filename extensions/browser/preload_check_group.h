// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PRELOAD_CHECK_GROUP_H_
#define EXTENSIONS_BROWSER_PRELOAD_CHECK_GROUP_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "extensions/browser/preload_check.h"

namespace extensions {

// PreloadCheckGroup runs a collection of other PreloadChecks and reports their
// collective status once they have all finished. To stop the remaining checks
// upon hitting the first error, use set_stop_on_first_error().
class PreloadCheckGroup : public PreloadCheck {
 public:
  PreloadCheckGroup();
  ~PreloadCheckGroup() override;

  // Adds a check to run. Not owned. Must be called before Start().
  void AddCheck(PreloadCheck* check);

  // PreloadCheck:
  void Start(ResultCallback callback) override;

  void set_stop_on_first_error(bool value) { stop_on_first_error_ = value; }

 private:
  // Saves any errors and may invoke the callback.
  virtual void OnCheckComplete(const Errors& errors);

  // Invokes the callback if the checks are considered finished.
  void MaybeInvokeCallback();

  base::ThreadChecker thread_checker_;

  // If true, the callback is invoked early when the first check fails,
  // stopping the remaining checks.
  bool stop_on_first_error_ = false;

  // Checks to run. Not owned.
  std::vector<PreloadCheck*> checks_;

  ResultCallback callback_;
  int running_checks_ = 0;
  Errors errors_;

  base::WeakPtrFactory<PreloadCheckGroup> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PreloadCheckGroup);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PRELOAD_CHECK_GROUP_H_
