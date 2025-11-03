// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_sync_channel.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/message_loop/message_pump_type.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;

namespace IPC {
namespace {

// Base class for a "process" with listener and IPC threads.
class Worker : public Listener {
 public:
  // Will create a channel without a name.
  Worker(Channel::Mode mode,
         const std::string& thread_name,
         mojo::ScopedMessagePipeHandle channel_handle)
      : done_(
            new WaitableEvent(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED)),
        channel_created_(
            new WaitableEvent(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED)),
        channel_handle_(std::move(channel_handle)),
        mode_(mode),
        ipc_thread_(
            std::make_unique<base::Thread>((thread_name + "_ipc").c_str())),
        listener_thread_((thread_name + "_listener").c_str()),
        overrided_thread_(nullptr),
        shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
        is_shutdown_(false) {}

  // Will create a named channel and use this name for the threads' name.
  Worker(mojo::ScopedMessagePipeHandle channel_handle, Channel::Mode mode)
      : done_(
            new WaitableEvent(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED)),
        channel_created_(
            new WaitableEvent(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED)),
        channel_handle_(std::move(channel_handle)),
        mode_(mode),
        ipc_thread_(std::make_unique<base::Thread>("ipc thread")),
        listener_thread_("listener thread"),
        overrided_thread_(nullptr),
        shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
        is_shutdown_(false) {}

  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;

  ~Worker() override {
    // Shutdown() must be called before destruction.
    CHECK(is_shutdown_);
  }
  void WaitForChannelCreation() { channel_created_->Wait(); }
  void CloseChannel() {
    DCHECK(ListenerThread()->task_runner()->BelongsToCurrentThread());
    channel_->Close();
  }
  void Start() {
    StartThread(&listener_thread_, base::MessagePumpType::DEFAULT);
    ListenerThread()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&Worker::OnStart, base::Unretained(this)));
  }
  void Shutdown() {
    // The IPC thread needs to outlive SyncChannel. We can't do this in
    // ~Worker(), since that'll reset the vtable pointer (to Worker's), which
    // may result in a race conditions. See http://crbug.com/25841.
    WaitableEvent listener_done(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    ListenerThread()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&Worker::OnListenerThreadShutdown1,
                                  base::Unretained(this), &listener_done));
    listener_done.Wait();
    listener_thread_.Stop();
    is_shutdown_ = true;
  }
  void OverrideThread(base::Thread* overrided_thread) {
    DCHECK(!overrided_thread_);
    overrided_thread_ = overrided_thread;
  }
  mojo::MessagePipeHandle TakeChannelHandle() {
    DCHECK(channel_handle_.is_valid());
    return channel_handle_.release();
  }
  Channel::Mode mode() { return mode_; }
  WaitableEvent* done_event() { return done_.get(); }
  void ResetChannel() { channel_.reset(); }
  // Derived classes need to call this when they've completed their part of
  // the test.
  void Done() { done_->Signal(); }

 protected:
  SyncChannel* channel() { return channel_.get(); }
  // Functions for derived classes to implement if they wish.
  virtual void Run() { }

  virtual SyncChannel* CreateChannel() {
    std::unique_ptr<SyncChannel> channel = SyncChannel::Create(
        this, ipc_thread_->task_runner(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), &shutdown_event_);

    channel->Init(TakeChannelHandle(), mode_, true);
    return channel.release();
  }

  base::Thread* ListenerThread() {
    return overrided_thread_ ? overrided_thread_.get() : &listener_thread_;
  }

  const base::Thread& ipc_thread() const { return *ipc_thread_.get(); }

 private:
  // Called on the listener thread to create the sync channel.
  void OnStart() {
    // Link ipc_thread_, listener_thread_ and channel_ altogether.
    StartThread(ipc_thread_.get(), base::MessagePumpType::IO);
    channel_.reset(CreateChannel());
    channel_created_->Signal();
    Run();
  }

  void OnListenerThreadShutdown1(WaitableEvent* listener_event) {
    WaitableEvent ipc_event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
    // SyncChannel needs to be destructed on the thread that it was created on.
    channel_.reset();

    base::RunLoop().RunUntilIdle();

    ipc_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&Worker::OnIPCThreadShutdown, base::Unretained(this),
                       listener_event, &ipc_event));
    ipc_event.Wait();
    // This destructs `ipc_thread_` on the listener thread.
    ipc_thread_.reset();

    listener_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&Worker::OnListenerThreadShutdown2,
                                  base::Unretained(this), listener_event));
  }

  void OnIPCThreadShutdown(WaitableEvent* listener_event,
                           WaitableEvent* ipc_event) {
    base::RunLoop().RunUntilIdle();
    ipc_event->Signal();
  }

  void OnListenerThreadShutdown2(WaitableEvent* listener_event) {
    base::RunLoop().RunUntilIdle();
    listener_event->Signal();
  }

  void StartThread(base::Thread* thread, base::MessagePumpType type) {
    base::Thread::Options options;
    options.message_pump_type = type;
    thread->StartWithOptions(std::move(options));
  }

  std::unique_ptr<WaitableEvent> done_;
  std::unique_ptr<WaitableEvent> channel_created_;
  mojo::ScopedMessagePipeHandle channel_handle_;
  Channel::Mode mode_;
  std::unique_ptr<SyncChannel> channel_;
  // This thread is constructed on the main thread, Start() on
  // `listener_thread_`, and therefore destructed/Stop()'d on the
  // `listener_thread_` too.
  std::unique_ptr<base::Thread> ipc_thread_;
  base::Thread listener_thread_;
  raw_ptr<base::Thread> overrided_thread_;

  base::WaitableEvent shutdown_event_;

  bool is_shutdown_;
};

class IPCSyncChannelTest : public testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

//------------------------------------------------------------------------------

// This class provides functionality to test the case that a Send on the sync
// channel does not crash after the channel has been closed.
class ServerSendAfterClose : public Worker {
 public:
  explicit ServerSendAfterClose(mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_SERVER,
               "simpler_server",
               std::move(channel_handle)),
        send_result_(true) {}

  bool send_result() const {
    return send_result_;
  }

 private:
  void Run() override {
    CloseChannel();
    Done();
  }

  bool send_result_;
};

// Test the case when the channel is closed and a Send is attempted after that.
TEST_F(IPCSyncChannelTest, SendAfterClose) {
  mojo::MessagePipe pipe;
  ServerSendAfterClose server(std::move(pipe.handle0));
  server.Start();

  server.done_event()->Wait();
  server.done_event()->Reset();
  server.Shutdown();
}

}  // namespace
}  // namespace IPC
