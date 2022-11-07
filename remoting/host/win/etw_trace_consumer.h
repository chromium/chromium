// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_ETW_TRACE_CONSUMER_H_
#define REMOTING_HOST_WIN_ETW_TRACE_CONSUMER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"

namespace remoting {

class AutoThreadTaskRunner;
class HostEventLogger;

class EtwTraceConsumer {
 public:
  virtual ~EtwTraceConsumer() = default;

  // Creates an ETW Trace Consumer which listens for Host ETW events.
  // Listening starts as soon as an instance is created and stops when the
  // instance is destroyed.  Only one instance can be active at a time.
  static std::unique_ptr<EtwTraceConsumer> Create(
      scoped_refptr<AutoThreadTaskRunner> task_runner,
      std::vector<std::unique_ptr<HostEventLogger>> loggers);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_ETW_TRACE_CONSUMER_H_
