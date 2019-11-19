// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_SESSION_TEST_UTIL_H_
#define NET_SPDY_SPDY_SESSION_TEST_UTIL_H_

#include <stdint.h>

#include <string>

#include "base/pending_task.h"
#include "base/task/task_observer.h"

namespace net {

// SpdySessionTestTaskObserver is a TaskObserver that monitors the
// completion of all tasks executed by the current MessageLoop, recording the
// number of tasks that refer to a specific function and filename.
class SpdySessionTestTaskObserver : public base::TaskObserver {
 public:
  // Creates a SpdySessionTaskObserver that will record all tasks that are
  // executed that were posted by the function named by |function_name|, located
  // in the file |file_name|.
  // Example:
  //  file_name = "foo.cc"
  //  function = "DoFoo"
  SpdySessionTestTaskObserver(const std::string& file_name,
                              const std::string& function_name);
  ~SpdySessionTestTaskObserver() override;

  // Implements TaskObserver.
  void WillProcessTask(const base::PendingTask& pending_task) override;
  void DidProcessTask(const base::PendingTask& pending_task) override;

  // Returns the number of tasks posted by the given function and file.
  uint16_t executed_count() const { return executed_count_; }

 private:
  uint16_t executed_count_;
  std::string file_name_;
  std::string function_name_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_SESSION_TEST_UTIL_H_
