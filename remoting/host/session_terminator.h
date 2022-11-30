// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SESSION_TERMINATOR_H_
#define REMOTING_HOST_SESSION_TERMINATOR_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

// Helper class that will terminate (log out) the current user session in its
// destructor.
class SessionTerminator {
 public:
  SessionTerminator(const SessionTerminator&) = delete;
  SessionTerminator& operator=(const SessionTerminator&) = delete;

  virtual ~SessionTerminator() = default;

  // Creates a session terminator for the current platform / host architecture.
  static std::unique_ptr<SessionTerminator> Create(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

 protected:
  SessionTerminator() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SESSION_TERMINATOR_H_
