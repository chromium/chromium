// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_CONTEXT_H_
#define REMOTING_CLIENT_CLIENT_CONTEXT_H_

#include "base/threading/thread.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// A class that manages threads and running context for the chromoting client
// process.
class ClientContext {
 public:
  ClientContext(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner);

  ClientContext(const ClientContext&) = delete;
  ClientContext& operator=(const ClientContext&) = delete;

  virtual ~ClientContext();

  void Start();
  void Stop();

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner() const;
  scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner() const;
  scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner() const;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // A thread that handles all video decode operations.
  base::Thread decode_thread_;

  // A thread that handles all audio decode operations.
  base::Thread audio_decode_thread_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_CONTEXT_H_
