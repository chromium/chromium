// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_IN_PROCESS_UTILITY_THREAD_H_
#define CONTENT_UTILITY_IN_PROCESS_UTILITY_THREAD_H_

#include <memory>

#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/common/in_process_child_thread_params.h"

namespace content {

class ChildProcess;

class InProcessUtilityThread : public base::Thread {
 public:
  InProcessUtilityThread(const InProcessChildThreadParams& params);

  InProcessUtilityThread(const InProcessUtilityThread&) = delete;
  InProcessUtilityThread& operator=(const InProcessUtilityThread&) = delete;

  ~InProcessUtilityThread() override;

 private:
  // base::Thread implementation:
  void Init() override;
  void CleanUp() override;

  void InitInternal();

  InProcessChildThreadParams params_;
  std::unique_ptr<ChildProcess> child_process_;
};

CONTENT_EXPORT base::Thread* CreateInProcessUtilityThread(
    const InProcessChildThreadParams& params);

}  // namespace content

#endif  // CONTENT_UTILITY_IN_PROCESS_UTILITY_THREAD_H_
