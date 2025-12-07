// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ANDROID_SEQUENCE_CHECKER_HELPER_H_
#define SERVICES_ON_DEVICE_MODEL_ANDROID_SEQUENCE_CHECKER_HELPER_H_

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class SequencedTaskRunner;
}

namespace on_device_model {

// A helper to ensure a task is run on the sequence where this helper is
// created.
class SequenceCheckerHelper {
 public:
  SequenceCheckerHelper();
  ~SequenceCheckerHelper();

  // Posts a task to the sequence where this object was created. If it's already
  // on the same sequence, the task will be run directly.
  void PostTask(const base::Location& from_here, base::OnceClosure task);

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ANDROID_SEQUENCE_CHECKER_HELPER_H_
