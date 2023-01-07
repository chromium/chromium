// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_EVENT_REPORTER_H_
#define REMOTING_HOST_HOST_EVENT_REPORTER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"

namespace remoting {

class HostStatusMonitor;

class HostEventReporter {
 public:
  // Creates an event-reporter that monitors host status changes and
  // forwards corresponding events to Encrypted Reporting Pipeline.
  static std::unique_ptr<HostEventReporter> Create(
      scoped_refptr<HostStatusMonitor> monitor);

  HostEventReporter(const HostEventReporter&) = delete;
  HostEventReporter& operator=(const HostEventReporter&) = delete;

  virtual ~HostEventReporter() = default;

 protected:
  HostEventReporter() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_EVENT_REPORTER_H_
