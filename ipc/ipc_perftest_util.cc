// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_perftest_util.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_perftest_messages.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/test/multiprocess_test_helper.h"

namespace IPC {

scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() {
  scoped_refptr<base::TaskRunner> runner = mojo::core::GetIOTaskRunner();
  return scoped_refptr<base::SingleThreadTaskRunner>(
      static_cast<base::SingleThreadTaskRunner*>(runner.get()));
}

ChannelReflectorListener::ChannelReflectorListener() : channel_(NULL) {
  VLOG(1) << "Client listener up";
}

ChannelReflectorListener::~ChannelReflectorListener() {
  VLOG(1) << "Client listener down";
}

void ChannelReflectorListener::Init(Sender* channel,
                                    const base::Closure& quit_closure) {
  DCHECK(!channel_);
  channel_ = channel;
  quit_closure_ = quit_closure;
}

bool ChannelReflectorListener::OnMessageReceived(const Message& message) {
  CHECK(channel_);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChannelReflectorListener, message)
    IPC_MESSAGE_HANDLER(TestMsg_Hello, OnHello)
    IPC_MESSAGE_HANDLER(TestMsg_Ping, OnPing)
    IPC_MESSAGE_HANDLER(TestMsg_SyncPing, OnSyncPing)
    IPC_MESSAGE_HANDLER(TestMsg_Quit, OnQuit)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ChannelReflectorListener::OnHello() {
  channel_->Send(new TestMsg_Hello);
}

void ChannelReflectorListener::OnPing(const std::string& payload) {
  channel_->Send(new TestMsg_Ping(payload));
}

void ChannelReflectorListener::OnSyncPing(const std::string& payload,
                                          std::string* response) {
  *response = payload;
}

void ChannelReflectorListener::OnQuit() {
  quit_closure_.Run();
}

void ChannelReflectorListener::Send(IPC::Message* message) {
  channel_->Send(message);
}

LockThreadAffinity::LockThreadAffinity(int cpu_number)
    : affinity_set_ok_(false) {
#if defined(OS_WIN)
  const DWORD_PTR thread_mask = static_cast<DWORD_PTR>(1) << cpu_number;
  old_affinity_ = SetThreadAffinityMask(GetCurrentThread(), thread_mask);
  affinity_set_ok_ = old_affinity_ != 0;
#elif defined(OS_LINUX)
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu_number, &cpuset);
  auto get_result = sched_getaffinity(0, sizeof(old_cpuset_), &old_cpuset_);
  DCHECK_EQ(0, get_result);
  auto set_result = sched_setaffinity(0, sizeof(cpuset), &cpuset);
  // Check for get_result failure, even though it should always succeed.
  affinity_set_ok_ = (set_result == 0) && (get_result == 0);
#endif
  if (!affinity_set_ok_)
    LOG(WARNING) << "Failed to set thread affinity to CPU " << cpu_number;
}

LockThreadAffinity::~LockThreadAffinity() {
  if (!affinity_set_ok_)
    return;
#if defined(OS_WIN)
  auto set_result = SetThreadAffinityMask(GetCurrentThread(), old_affinity_);
  DCHECK_NE(0u, set_result);
#elif defined(OS_LINUX)
  auto set_result = sched_setaffinity(0, sizeof(old_cpuset_), &old_cpuset_);
  DCHECK_EQ(0, set_result);
#endif
}

MojoPerfTestClient::MojoPerfTestClient()
    : listener_(new ChannelReflectorListener()) {
  mojo::core::test::MultiprocessTestHelper::ChildSetup();
}

MojoPerfTestClient::~MojoPerfTestClient() = default;

int MojoPerfTestClient::Run(MojoHandle handle) {
  handle_ = mojo::MakeScopedHandle(mojo::MessagePipeHandle(handle));
  LockThreadAffinity thread_locker(kSharedCore);

  base::RunLoop run_loop;
  std::unique_ptr<ChannelProxy> channel = IPC::ChannelProxy::Create(
      handle_.release(), Channel::MODE_CLIENT, listener_.get(),
      GetIOThreadTaskRunner(), base::ThreadTaskRunnerHandle::Get());
  listener_->Init(channel.get(), run_loop.QuitWhenIdleClosure());
  run_loop.Run();
  return 0;
}

ReflectorImpl::ReflectorImpl(mojo::ScopedMessagePipeHandle handle,
                             const base::Closure& quit_closure)
    : quit_closure_(quit_closure),
      receiver_(
          this,
          mojo::PendingReceiver<IPC::mojom::Reflector>(std::move(handle))) {}

ReflectorImpl::~ReflectorImpl() {
  ignore_result(receiver_.Unbind().PassPipe().release());
}

void ReflectorImpl::Ping(const std::string& value, PingCallback callback) {
  std::move(callback).Run(value);
}

void ReflectorImpl::SyncPing(const std::string& value, PingCallback callback) {
  std::move(callback).Run(value);
}

void ReflectorImpl::Quit() {
  if (quit_closure_)
    quit_closure_.Run();
}

}  // namespace IPC
