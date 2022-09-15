// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_context.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

ClientContext::ClientContext(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : main_task_runner_(main_task_runner),
      decode_thread_("ChromotingClientDecodeThread"),
      audio_decode_thread_("ChromotingClientAudioDecodeThread") {
}

ClientContext::~ClientContext() = default;

void ClientContext::Start() {
  // Start all the threads.
  decode_thread_.Start();
  audio_decode_thread_.Start();
}

void ClientContext::Stop() {
  // Stop all the threads.
  decode_thread_.Stop();
  audio_decode_thread_.Stop();
}

scoped_refptr<base::SingleThreadTaskRunner> ClientContext::main_task_runner()
    const {
  return main_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner> ClientContext::decode_task_runner()
    const {
  return decode_thread_.task_runner();
}

scoped_refptr<base::SingleThreadTaskRunner>
ClientContext::audio_decode_task_runner() const {
  return audio_decode_thread_.task_runner();
}

}  // namespace remoting
