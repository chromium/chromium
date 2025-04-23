// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/common/remoting_client.h"

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"

namespace remoting {

RemotingClient::RemotingClient() = default;
RemotingClient::~RemotingClient() = default;

void RemotingClient::StartSession(std::string_view support_id,
                                  std::string_view access_token) {
  base::RunLoop run_loop;

  base::ThreadPool::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                    base::Seconds(3));

  run_loop.Run();
}

}  // namespace remoting
