// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EVENT_LOGGER_H_
#define REMOTING_HOST_HOST_EVENT_LOGGER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"

namespace remoting {

class HostStatusMonitor;

class HostEventLogger {
 public:
  HostEventLogger(const HostEventLogger&) = delete;
  HostEventLogger& operator=(const HostEventLogger&) = delete;

  virtual ~HostEventLogger() {}

  // Creates an event-logger that monitors host status changes and logs
  // corresponding events to the OS-specific log (syslog/EventLog).
  static std::unique_ptr<HostEventLogger> Create(
      scoped_refptr<HostStatusMonitor> monitor,
      const std::string& application_name);

 protected:
  HostEventLogger() {}
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_EVENT_LOGGER_H_
