// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SCOPED_URL_FORWARDER_LINUX_H_
#define REMOTING_HOST_SCOPED_URL_FORWARDER_LINUX_H_

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "remoting/host/scoped_url_forwarder.h"

namespace remoting {

// Linux implementation of ScopedUrlForwarder.
class ScopedUrlForwarderLinux final : public ScopedUrlForwarder {
 public:
  explicit ScopedUrlForwarderLinux(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~ScopedUrlForwarderLinux() override;

  ScopedUrlForwarderLinux(const ScopedUrlForwarderLinux&) = delete;
  ScopedUrlForwarderLinux& operator=(const ScopedUrlForwarderLinux&) = delete;

 private:
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SCOPED_URL_FORWARDER_LINUX_H_
