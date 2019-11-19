// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/preload_check_group.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "extensions/common/extension.h"

namespace extensions {

PreloadCheckGroup::PreloadCheckGroup() : PreloadCheck(nullptr) {}

PreloadCheckGroup::~PreloadCheckGroup() {}

void PreloadCheckGroup::AddCheck(PreloadCheck* check) {
  DCHECK_EQ(0, running_checks_);
  checks_.push_back(check);
}

void PreloadCheckGroup::Start(ResultCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(0, running_checks_);

  callback_ = std::move(callback);
  running_checks_ = checks_.size();
  for (auto* check : checks_) {
    check->Start(base::Bind(&PreloadCheckGroup::OnCheckComplete,
                            weak_ptr_factory_.GetWeakPtr()));
    // Synchronous checks may fail immediately.
    if (running_checks_ == 0)
      return;
  }
}

void PreloadCheckGroup::OnCheckComplete(const Errors& errors) {
  DCHECK(thread_checker_.CalledOnValidThread());
  errors_.insert(errors.begin(), errors.end());
  running_checks_--;
  MaybeInvokeCallback();
}

void PreloadCheckGroup::MaybeInvokeCallback() {
  // Only invoke callback if all checks are complete, or if there was at least
  // one failure and |stop_on_first_error_| is true.
  if (running_checks_ > 0 && (errors_.empty() || !stop_on_first_error_))
    return;

  // If we are failing fast, discard any pending results.
  weak_ptr_factory_.InvalidateWeakPtrs();
  running_checks_ = 0;

  DCHECK(callback_);
  std::move(callback_).Run(errors_);
}

}  // namespace extensions
