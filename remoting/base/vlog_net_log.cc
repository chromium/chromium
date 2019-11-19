// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/vlog_net_log.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/log/net_log_entry.h"

namespace remoting {

class VlogNetLog::Observer : public net::NetLog::ThreadSafeObserver {
 public:
  Observer();
  ~Observer() override;

  // NetLog::ThreadSafeObserver overrides:
  void OnAddEntry(const net::NetLogEntry& entry) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Observer);
};

VlogNetLog::Observer::Observer() = default;

VlogNetLog::Observer::~Observer() = default;

void VlogNetLog::Observer::OnAddEntry(const net::NetLogEntry& entry) {
  if (VLOG_IS_ON(4)) {
    base::Value value = entry.ToValue();
    std::string json;
    base::JSONWriter::Write(value, &json);
    VLOG(4) << json;
  }
}

VlogNetLog::VlogNetLog()
    : observer_(new Observer()) {
  AddObserver(observer_.get(), net::NetLogCaptureMode::kIncludeSensitive);
}

VlogNetLog::~VlogNetLog() {
  RemoveObserver(observer_.get());
}

}  // namespace remoting
