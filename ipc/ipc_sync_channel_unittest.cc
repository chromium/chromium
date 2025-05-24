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
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_sync_message_unittest.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;

namespace IPC {
namespace {

// Base class for a "process" with listener and IPC threads.
class Worker : public Listener, public Sender {
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
  bool Send(Message* msg) override { return channel_->Send(msg); }
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
  bool SendAnswerToLife(bool succeed) {
    int answer = 0;
    SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
    bool result = Send(msg);
    DCHECK_EQ(result, succeed);
    DCHECK_EQ(answer, (succeed ? 42 : 0));
    return result;
  }
  bool SendDouble(bool succeed) {
    int answer = 0;
    SyncMessage* msg = new SyncChannelTestMsg_Double(5, &answer);
    bool result = Send(msg);
    DCHECK_EQ(result, succeed);
    DCHECK_EQ(answer, (succeed ? 10 : 0));
    return result;
  }
  mojo::MessagePipeHandle TakeChannelHandle() {
    DCHECK(channel_handle_.is_valid());
    return channel_handle_.release();
  }
  Channel::Mode mode() { return mode_; }
  WaitableEvent* done_event() { return done_.get(); }
  WaitableEvent* shutdown_event() { return &shutdown_event_; }
  void ResetChannel() { channel_.reset(); }
  // Derived classes need to call this when they've completed their part of
  // the test.
  void Done() { done_->Signal(); }

 protected:
  SyncChannel* channel() { return channel_.get(); }
  // Functions for derived classes to implement if they wish.
  virtual void Run() { }
  virtual void OnAnswer(int* answer) { NOTREACHED(); }
  virtual void OnAnswerDelay(Message* reply_msg) {
    // The message handler map below can only take one entry for
    // SyncChannelTestMsg_AnswerToLife, so since some classes want
    // the normal version while other want the delayed reply, we
    // call the normal version if the derived class didn't override
    // this function.
    int answer;
    OnAnswer(&answer);
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, answer);
    Send(reply_msg);
  }
  virtual void OnDouble(int in, int* out) { NOTREACHED(); }
  virtual void OnDoubleDelay(int in, Message* reply_msg) {
    int result;
    OnDouble(in, &result);
    SyncChannelTestMsg_Double::WriteReplyParams(reply_msg, result);
    Send(reply_msg);
  }

  virtual void OnNestedTestMsg(Message* reply_msg) { NOTREACHED(); }

  virtual SyncChannel* CreateChannel() {
    std::unique_ptr<SyncChannel> channel = SyncChannel::Create(
        TakeChannelHandle(), mode_, this, ipc_thread_->task_runner(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), true,
        &shutdown_event_);
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

  bool OnMessageReceived(const Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(Worker, message)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncChannelTestMsg_Double, OnDoubleDelay)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncChannelTestMsg_AnswerToLife,
                                     OnAnswerDelay)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncChannelNestedTestMsg_String,
                                     OnNestedTestMsg)
    IPC_END_MESSAGE_MAP()
    return true;
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


// Starts the test with the given workers.  This function deletes the workers
// when it's done.
void RunTest(std::vector<Worker*> workers) {
  // First we create the workers that are channel servers, or else the other
  // workers' channel initialization might fail because the pipe isn't created..
  for (size_t i = 0; i < workers.size(); ++i) {
    if (workers[i]->mode() & Channel::MODE_SERVER_FLAG) {
      workers[i]->Start();
      workers[i]->WaitForChannelCreation();
    }
  }

  // now create the clients
  for (size_t i = 0; i < workers.size(); ++i) {
    if (workers[i]->mode() & Channel::MODE_CLIENT_FLAG)
      workers[i]->Start();
  }

  // wait for all the workers to finish
  for (size_t i = 0; i < workers.size(); ++i)
    workers[i]->done_event()->Wait();

  for (size_t i = 0; i < workers.size(); ++i) {
    workers[i]->Shutdown();
    delete workers[i];
  }
}

class IPCSyncChannelTest : public testing::Test {
 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

//------------------------------------------------------------------------------

class SimpleServer : public Worker {
 public:
  explicit SimpleServer(mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_SERVER,
               "simpler_server",
               std::move(channel_handle)) {}
  void Run() override {
    SendAnswerToLife(true);
    Done();
  }
};

class SimpleClient : public Worker {
 public:
  explicit SimpleClient(mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_CLIENT,
               "simple_client",
               std::move(channel_handle)) {}

  void OnAnswer(int* answer) override {
    *answer = 42;
    Done();
  }
};

void Simple() {
  std::vector<Worker*> workers;
  mojo::MessagePipe pipe;
  workers.push_back(new SimpleServer(std::move(pipe.handle0)));
  workers.push_back(new SimpleClient(std::move(pipe.handle1)));
  RunTest(workers);
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Simple DISABLED_Simple
#else
#define MAYBE_Simple Simple
#endif
// Tests basic synchronous call
TEST_F(IPCSyncChannelTest, MAYBE_Simple) {
  Simple();
}

//------------------------------------------------------------------------------

// Worker classes which override how the sync channel is created to use the
// two-step initialization (calling the lightweight constructor and then
// ChannelProxy::Init separately) process.
class TwoStepServer : public Worker {
 public:
  TwoStepServer(bool create_pipe_now,
                mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_SERVER,
               "simpler_server",
               std::move(channel_handle)),
        create_pipe_now_(create_pipe_now) {}

  void Run() override {
    SendAnswerToLife(true);
    Done();
  }

  SyncChannel* CreateChannel() override {
    SyncChannel* channel =
        SyncChannel::Create(TakeChannelHandle(), mode(), this,
                            ipc_thread().task_runner(),
                            base::SingleThreadTaskRunner::GetCurrentDefault(),
                            create_pipe_now_, shutdown_event())
            .release();
    return channel;
  }

  bool create_pipe_now_;
};

class TwoStepClient : public Worker {
 public:
  TwoStepClient(bool create_pipe_now,
                mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_CLIENT,
               "simple_client",
               std::move(channel_handle)),
        create_pipe_now_(create_pipe_now) {}

  void OnAnswer(int* answer) override {
    *answer = 42;
    Done();
  }

  SyncChannel* CreateChannel() override {
    SyncChannel* channel =
        SyncChannel::Create(TakeChannelHandle(), mode(), this,
                            ipc_thread().task_runner(),
                            base::SingleThreadTaskRunner::GetCurrentDefault(),
                            create_pipe_now_, shutdown_event())
            .release();
    return channel;
  }

  bool create_pipe_now_;
};

void TwoStep(bool create_server_pipe_now, bool create_client_pipe_now) {
  std::vector<Worker*> workers;
  mojo::MessagePipe pipe;
  workers.push_back(
      new TwoStepServer(create_server_pipe_now, std::move(pipe.handle0)));
  workers.push_back(
      new TwoStepClient(create_client_pipe_now, std::move(pipe.handle1)));
  RunTest(workers);
}

// Tests basic two-step initialization, where you call the lightweight
// constructor then Init.
TEST_F(IPCSyncChannelTest, TwoStepInitialization) {
  TwoStep(false, false);
  TwoStep(false, true);
  TwoStep(true, false);
  TwoStep(true, true);
}

//------------------------------------------------------------------------------

class DelayClient : public Worker {
 public:
  explicit DelayClient(mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_CLIENT,
               "delay_client",
               std::move(channel_handle)) {}

  void OnAnswerDelay(Message* reply_msg) override {
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
    Send(reply_msg);
    Done();
  }
};

void DelayReply() {
  std::vector<Worker*> workers;
  mojo::MessagePipe pipe;
  workers.push_back(new SimpleServer(std::move(pipe.handle0)));
  workers.push_back(new DelayClient(std::move(pipe.handle1)));
  RunTest(workers);
}

// Tests that asynchronous replies work
TEST_F(IPCSyncChannelTest, DelayReply) {
  DelayReply();
}

//------------------------------------------------------------------------------

class NoHangServer : public Worker {
 public:
  NoHangServer(WaitableEvent* got_first_reply,
               mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_SERVER,
               "no_hang_server",
               std::move(channel_handle)),
        got_first_reply_(got_first_reply) {}
  void Run() override {
    SendAnswerToLife(true);
    got_first_reply_->Signal();

    SendAnswerToLife(false);
    Done();
  }

  raw_ptr<WaitableEvent> got_first_reply_;
};

class NoHangClient : public Worker {
 public:
  NoHangClient(WaitableEvent* got_first_reply,
               mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_CLIENT,
               "no_hang_client",
               std::move(channel_handle)),
        got_first_reply_(got_first_reply) {}

  void OnAnswerDelay(Message* reply_msg) override {
    // Use the DELAY_REPLY macro so that we can force the reply to be sent
    // before this function returns (when the channel will be reset).
    SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
    Send(reply_msg);
    got_first_reply_->Wait();
    CloseChannel();
    Done();
  }

  raw_ptr<WaitableEvent> got_first_reply_;
};

void NoHang() {
  WaitableEvent got_first_reply(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::vector<Worker*> workers;
  mojo::MessagePipe pipe;
  workers.push_back(
      new NoHangServer(&got_first_reply, std::move(pipe.handle0)));
  workers.push_back(
      new NoHangClient(&got_first_reply, std::move(pipe.handle1)));
  RunTest(workers);
}

// Tests that caller doesn't hang if receiver dies
TEST_F(IPCSyncChannelTest, NoHang) {
  NoHang();
}

//------------------------------------------------------------------------------

class UnblockServer : public Worker {
 public:
  UnblockServer(bool delete_during_send,
                mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_SERVER,
               "unblock_server",
               std::move(channel_handle)),
        delete_during_send_(delete_during_send) {}
  void Run() override {
    if (delete_during_send_) {
      // Use custom code since race conditions mean the answer may or may not be
      // available.
      int answer = 0;
      SyncMessage* msg = new SyncChannelTestMsg_AnswerToLife(&answer);
      Send(msg);
    } else {
      SendAnswerToLife(true);
    }
    Done();
  }

  void OnDoubleDelay(int in, Message* reply_msg) override {
    SyncChannelTestMsg_Double::WriteReplyParams(reply_msg, in * 2);
    Send(reply_msg);
    if (delete_during_send_)
      ResetChannel();
  }

  bool delete_during_send_;
};

class UnblockClient : public Worker {
 public:
  explicit UnblockClient(mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_CLIENT,
               "unblock_client",
               std::move(channel_handle)) {}

  void OnAnswer(int* answer) override {
    SendDouble(true);
    *answer = 42;
    Done();
  }
};

void Unblock(bool delete_during_send) {
  std::vector<Worker*> workers;
  mojo::MessagePipe pipe;
  workers.push_back(
      new UnblockServer(delete_during_send, std::move(pipe.handle0)));
  workers.push_back(new UnblockClient(std::move(pipe.handle1)));
  RunTest(workers);
}

// Tests that the caller unblocks to answer a sync message from the receiver.
TEST_F(IPCSyncChannelTest, Unblock) {
  Unblock(false);
}

//------------------------------------------------------------------------------

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ChannelDeleteDuringSend DISABLED_ChannelDeleteDuringSend
#else
#define MAYBE_ChannelDeleteDuringSend ChannelDeleteDuringSend
#endif
// Tests that the the SyncChannel object can be deleted during a Send.
TEST_F(IPCSyncChannelTest, MAYBE_ChannelDeleteDuringSend) {
  Unblock(true);
}

//------------------------------------------------------------------------------

class RecursiveServer : public Worker {
 public:
  RecursiveServer(bool expected_send_result,
                  mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_SERVER,
               "recursive_server",
               std::move(channel_handle)),
        expected_send_result_(expected_send_result) {}
  void Run() override {
    SendDouble(expected_send_result_);
    Done();
  }

  void OnDouble(int in, int* out) override {
    *out = in * 2;
    SendAnswerToLife(expected_send_result_);
  }

  bool expected_send_result_;
};

class RecursiveClient : public Worker {
 public:
  RecursiveClient(bool close_channel,
                  mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_CLIENT,
               "recursive_client",
               std::move(channel_handle)),
        close_channel_(close_channel) {}

  void OnDoubleDelay(int in, Message* reply_msg) override {
    SendDouble(!close_channel_);
    if (close_channel_) {
      delete reply_msg;
    } else {
      SyncChannelTestMsg_Double::WriteReplyParams(reply_msg, in * 2);
      Send(reply_msg);
    }
    Done();
  }

  void OnAnswerDelay(Message* reply_msg) override {
    if (close_channel_) {
      delete reply_msg;
      CloseChannel();
    } else {
      SyncChannelTestMsg_AnswerToLife::WriteReplyParams(reply_msg, 42);
      Send(reply_msg);
    }
  }

  bool close_channel_;
};

void Recursive() {
  std::vector<Worker*> workers;
  mojo::MessagePipe pipe;
  workers.push_back(new RecursiveServer(true, std::move(pipe.handle0)));
  workers.push_back(new RecursiveClient(false, std::move(pipe.handle1)));
  RunTest(workers);
}

// Tests a server calling Send while another Send is pending.
TEST_F(IPCSyncChannelTest, Recursive) {
  Recursive();
}

//------------------------------------------------------------------------------

void RecursiveNoHang() {
  std::vector<Worker*> workers;
  mojo::MessagePipe pipe;
  workers.push_back(new RecursiveServer(false, std::move(pipe.handle0)));
  workers.push_back(new RecursiveClient(true, std::move(pipe.handle1)));
  RunTest(workers);
}

// Tests that if a caller makes a sync call during an existing sync call and
// the receiver dies, neither of the Send() calls hang.
TEST_F(IPCSyncChannelTest, RecursiveNoHang) {
  RecursiveNoHang();
}

//------------------------------------------------------------------------------

class MultipleServer1 : public Worker {
 public:
  explicit MultipleServer1(mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(std::move(channel_handle), Channel::MODE_SERVER) {}

  void Run() override {
    SendDouble(true);
    Done();
  }
};

class MultipleClient1 : public Worker {
 public:
  MultipleClient1(WaitableEvent* client1_msg_received,
                  WaitableEvent* client1_can_reply,
                  mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(std::move(channel_handle), Channel::MODE_CLIENT),
        client1_msg_received_(client1_msg_received),
        client1_can_reply_(client1_can_reply) {}

  void OnDouble(int in, int* out) override {
    client1_msg_received_->Signal();
    *out = in * 2;
    client1_can_reply_->Wait();
    Done();
  }

 private:
  raw_ptr<WaitableEvent> client1_msg_received_;
  raw_ptr<WaitableEvent> client1_can_reply_;
};

class MultipleServer2 : public Worker {
 public:
  explicit MultipleServer2(mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(std::move(channel_handle), Channel::MODE_SERVER) {}

  void OnAnswer(int* result) override {
    *result = 42;
    Done();
  }
};

class MultipleClient2 : public Worker {
 public:
  MultipleClient2(WaitableEvent* client1_msg_received,
                  WaitableEvent* client1_can_reply,
                  mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(std::move(channel_handle), Channel::MODE_CLIENT),
        client1_msg_received_(client1_msg_received),
        client1_can_reply_(client1_can_reply) {}

  void Run() override {
    client1_msg_received_->Wait();
    SendAnswerToLife(true);
    client1_can_reply_->Signal();
    Done();
  }

 private:
  raw_ptr<WaitableEvent> client1_msg_received_;
  raw_ptr<WaitableEvent> client1_can_reply_;
};

void Multiple() {
  std::vector<Worker*> workers;

  // A shared worker thread so that server1 and server2 run on one thread.
  base::Thread worker_thread("Multiple");
  ASSERT_TRUE(worker_thread.Start());

  // Server1 sends a sync msg to client1, which blocks the reply until
  // server2 (which runs on the same worker thread as server1) responds
  // to a sync msg from client2.
  WaitableEvent client1_msg_received(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent client1_can_reply(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  Worker* worker;

  mojo::MessagePipe pipe1, pipe2;
  worker = new MultipleServer2(std::move(pipe2.handle0));
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new MultipleClient2(&client1_msg_received, &client1_can_reply,
                               std::move(pipe2.handle1));
  workers.push_back(worker);

  worker = new MultipleServer1(std::move(pipe1.handle0));
  worker->OverrideThread(&worker_thread);
  workers.push_back(worker);

  worker = new MultipleClient1(&client1_msg_received, &client1_can_reply,
                               std::move(pipe1.handle1));
  workers.push_back(worker);

  RunTest(workers);
}

// Tests that multiple SyncObjects on the same listener thread can unblock each
// other.
TEST_F(IPCSyncChannelTest, Multiple) {
  Multiple();
}

//------------------------------------------------------------------------------

// This class provides server side functionality to test the case where
// multiple sync channels are in use on the same thread on the client.
class QueuedReplyServer : public Worker {
 public:
  QueuedReplyServer(base::Thread* listener_thread,
                    mojo::ScopedMessagePipeHandle channel_handle,
                    const std::string& reply_text)
      : Worker(std::move(channel_handle), Channel::MODE_SERVER),
        reply_text_(reply_text) {
    Worker::OverrideThread(listener_thread);
  }

  void OnNestedTestMsg(Message* reply_msg) override {
    VLOG(1) << __FUNCTION__ << " Sending reply: " << reply_text_;
    SyncChannelNestedTestMsg_String::WriteReplyParams(reply_msg, reply_text_);
    Send(reply_msg);
    Done();
  }

 private:
  std::string reply_text_;
};

// The QueuedReplyClient class provides functionality to test the case where
// multiple sync channels are in use on the same thread.
class QueuedReplyClient : public Worker {
 public:
  QueuedReplyClient(base::Thread* listener_thread,
                    mojo::ScopedMessagePipeHandle channel_handle,
                    const std::string& expected_text)
      : Worker(std::move(channel_handle), Channel::MODE_CLIENT),
        expected_text_(expected_text) {
    Worker::OverrideThread(listener_thread);
  }

  void Run() override {
    std::string response;
    SyncMessage* msg = new SyncChannelNestedTestMsg_String(&response);
    bool result = Send(msg);
    DCHECK(result);
    DCHECK_EQ(response, expected_text_);

    VLOG(1) << __FUNCTION__ << " Received reply: " << response;
    Done();
  }

 private:
  std::string expected_text_;
};

void QueuedReply() {
  std::vector<Worker*> workers;

  // A shared worker thread for servers
  base::Thread server_worker_thread("QueuedReply_ServerListener");
  ASSERT_TRUE(server_worker_thread.Start());

  base::Thread client_worker_thread("QueuedReply_ClientListener");
  ASSERT_TRUE(client_worker_thread.Start());

  Worker* worker;

  mojo::MessagePipe pipe1, pipe2;
  worker = new QueuedReplyServer(&server_worker_thread,
                                 std::move(pipe1.handle0), "Got first message");
  workers.push_back(worker);

  worker = new QueuedReplyServer(
      &server_worker_thread, std::move(pipe2.handle0), "Got second message");
  workers.push_back(worker);

  worker = new QueuedReplyClient(&client_worker_thread,
                                 std::move(pipe1.handle1), "Got first message");
  workers.push_back(worker);

  worker = new QueuedReplyClient(
      &client_worker_thread, std::move(pipe2.handle1), "Got second message");
  workers.push_back(worker);

  RunTest(workers);
}

// While a blocking send is in progress, the listener thread might answer other
// synchronous messages.  This tests that if during the response to another
// message the reply to the original messages comes, it is queued up correctly
// and the original Send is unblocked later.
TEST_F(IPCSyncChannelTest, QueuedReply) {
  QueuedReply();
}

//------------------------------------------------------------------------------

class TestSyncMessageFilter : public SyncMessageFilter {
 public:
  TestSyncMessageFilter(
      base::WaitableEvent* shutdown_event,
      Worker* worker,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : SyncMessageFilter(shutdown_event),
        worker_(worker),
        task_runner_(task_runner) {}

  void OnFilterAdded(Channel* channel) override {
    SyncMessageFilter::OnFilterAdded(channel);
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TestSyncMessageFilter::SendMessageOnHelperThread,
                       this));
  }

  void SendMessageOnHelperThread() {
    int answer = 0;
    bool result = Send(new SyncChannelTestMsg_AnswerToLife(&answer));
    DCHECK(result);
    DCHECK_EQ(answer, 42);

    worker_->Done();
  }

 private:
  ~TestSyncMessageFilter() override = default;

  raw_ptr<Worker> worker_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class SyncMessageFilterServer : public Worker {
 public:
  explicit SyncMessageFilterServer(mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_SERVER,
               "sync_message_filter_server",
               std::move(channel_handle)),
        thread_("helper_thread") {
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::DEFAULT;
    thread_.StartWithOptions(std::move(options));
    filter_ = new TestSyncMessageFilter(shutdown_event(), this,
                                        thread_.task_runner());
  }

  void Run() override {
    channel()->AddFilter(filter_.get());
  }

  base::Thread thread_;
  scoped_refptr<TestSyncMessageFilter> filter_;
};

// This class provides functionality to test the case that a Send on the sync
// channel does not crash after the channel has been closed.
class ServerSendAfterClose : public Worker {
 public:
  explicit ServerSendAfterClose(mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(Channel::MODE_SERVER,
               "simpler_server",
               std::move(channel_handle)),
        send_result_(true) {}

  bool SendDummy() {
    ListenerThread()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&ServerSendAfterClose::Send),
                       base::Unretained(this), new SyncChannelTestMsg_NoArgs));
    return true;
  }

  bool send_result() const {
    return send_result_;
  }

 private:
  void Run() override {
    CloseChannel();
    Done();
  }

  bool Send(Message* msg) override {
    send_result_ = Worker::Send(msg);
    Done();
    return send_result_;
  }

  bool send_result_;
};

// Tests basic synchronous call
TEST_F(IPCSyncChannelTest, SyncMessageFilter) {
  std::vector<Worker*> workers;
  mojo::MessagePipe pipe;
  workers.push_back(new SyncMessageFilterServer(std::move(pipe.handle0)));
  workers.push_back(new SimpleClient(std::move(pipe.handle1)));
  RunTest(workers);
}

// Test the case when the channel is closed and a Send is attempted after that.
TEST_F(IPCSyncChannelTest, SendAfterClose) {
  mojo::MessagePipe pipe;
  ServerSendAfterClose server(std::move(pipe.handle0));
  server.Start();

  server.done_event()->Wait();
  server.done_event()->Reset();

  server.SendDummy();
  server.done_event()->Wait();

  EXPECT_FALSE(server.send_result());

  server.Shutdown();
}

//------------------------------------------------------------------------------

class RestrictedDispatchServer : public Worker {
 public:
  RestrictedDispatchServer(WaitableEvent* sent_ping_event,
                           WaitableEvent* wait_event,
                           mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(std::move(channel_handle), Channel::MODE_SERVER),
        sent_ping_event_(sent_ping_event),
        wait_event_(wait_event) {}

  void OnDoPing(int ping) {
    // Send an asynchronous message that unblocks the caller.
    Message* msg = new SyncChannelTestMsg_Ping(ping);
    msg->set_unblock(true);
    Send(msg);
    // Signal the event after the message has been sent on the channel, on the
    // IPC thread.
    ipc_thread().task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&RestrictedDispatchServer::OnPingSent,
                                  base::Unretained(this)));
  }

  void OnPingTTL(int ping, int* out) {
    *out = ping;
    wait_event_->Wait();
  }

  base::Thread* ListenerThread() { return Worker::ListenerThread(); }

 private:
  bool OnMessageReceived(const Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(RestrictedDispatchServer, message)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_NoArgs, OnNoArgs)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_PingTTL, OnPingTTL)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_Done, Done)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnPingSent() {
    sent_ping_event_->Signal();
  }

  void OnNoArgs() { }
  raw_ptr<WaitableEvent> sent_ping_event_;
  raw_ptr<WaitableEvent> wait_event_;
};

class NonRestrictedDispatchServer : public Worker {
 public:
  NonRestrictedDispatchServer(WaitableEvent* signal_event,
                              mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(std::move(channel_handle), Channel::MODE_SERVER),
        signal_event_(signal_event) {}

  base::Thread* ListenerThread() { return Worker::ListenerThread(); }

  void OnDoPingTTL(int ping) {
    int value = 0;
    Send(new SyncChannelTestMsg_PingTTL(ping, &value));
    signal_event_->Signal();
  }

 private:
  bool OnMessageReceived(const Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(NonRestrictedDispatchServer, message)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_NoArgs, OnNoArgs)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_Done, Done)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnNoArgs() { }
  raw_ptr<WaitableEvent> signal_event_;
};

class RestrictedDispatchClient : public Worker {
 public:
  RestrictedDispatchClient(
      WaitableEvent* sent_ping_event,
      RestrictedDispatchServer* server,
      NonRestrictedDispatchServer* server2,
      int* success,
      mojo::ScopedMessagePipeHandle restricted_channel_handle,
      mojo::ScopedMessagePipeHandle non_restricted_channel_handle)
      : Worker(std::move(restricted_channel_handle), Channel::MODE_CLIENT),
        ping_(0),
        server_(server),
        server2_(server2),
        success_(success),
        sent_ping_event_(sent_ping_event),
        non_restricted_channel_handle_(
            std::move(non_restricted_channel_handle)) {}

  void Run() override {
    // Incoming messages from our channel should only be dispatched when we
    // send a message on that same channel.
    channel()->SetRestrictDispatchChannelGroup(1);

    server_->ListenerThread()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&RestrictedDispatchServer::OnDoPing,
                                  base::Unretained(server_), 1));
    sent_ping_event_->Wait();
    Send(new SyncChannelTestMsg_NoArgs);
    if (ping_ == 1)
      ++*success_;
    else
      LOG(ERROR) << "Send failed to dispatch incoming message on same channel";

    non_restricted_channel_ = SyncChannel::Create(
        non_restricted_channel_handle_.release(), IPC::Channel::MODE_CLIENT,
        this, ipc_thread().task_runner(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), true,
        shutdown_event());

    server_->ListenerThread()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&RestrictedDispatchServer::OnDoPing,
                                  base::Unretained(server_), 2));
    sent_ping_event_->Wait();
    // Check that the incoming message is *not* dispatched when sending on the
    // non restricted channel.
    // TODO(piman): there is a possibility of a false positive race condition
    // here, if the message that was posted on the server-side end of the pipe
    // is not visible yet on the client side, but I don't know how to solve this
    // without hooking into the internals of SyncChannel. I haven't seen it in
    // practice (i.e. not setting SetRestrictDispatchToSameChannel does cause
    // the following to fail).
    non_restricted_channel_->Send(new SyncChannelTestMsg_NoArgs);
    if (ping_ == 1)
      ++*success_;
    else
      LOG(ERROR) << "Send dispatched message from restricted channel";

    Send(new SyncChannelTestMsg_NoArgs);
    if (ping_ == 2)
      ++*success_;
    else
      LOG(ERROR) << "Send failed to dispatch incoming message on same channel";

    // Check that the incoming message on the non-restricted channel is
    // dispatched when sending on the restricted channel.
    server2_->ListenerThread()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&NonRestrictedDispatchServer::OnDoPingTTL,
                                  base::Unretained(server2_), 3));
    int value = 0;
    Send(new SyncChannelTestMsg_PingTTL(4, &value));
    if (ping_ == 3 && value == 4)
      ++*success_;
    else
      LOG(ERROR) << "Send failed to dispatch message from unrestricted channel";

    non_restricted_channel_->Send(new SyncChannelTestMsg_Done);
    non_restricted_channel_.reset();
    Send(new SyncChannelTestMsg_Done);
    Done();
  }

 private:
  bool OnMessageReceived(const Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(RestrictedDispatchClient, message)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_Ping, OnPing)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(SyncChannelTestMsg_PingTTL, OnPingTTL)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnPing(int ping) {
    ping_ = ping;
  }

  void OnPingTTL(int ping, IPC::Message* reply) {
    ping_ = ping;
    // This message comes from the NonRestrictedDispatchServer, we have to send
    // the reply back manually.
    SyncChannelTestMsg_PingTTL::WriteReplyParams(reply, ping);
    non_restricted_channel_->Send(reply);
  }

  int ping_;
  raw_ptr<RestrictedDispatchServer, DanglingUntriaged> server_;
  raw_ptr<NonRestrictedDispatchServer, DanglingUntriaged> server2_;
  raw_ptr<int> success_;
  raw_ptr<WaitableEvent> sent_ping_event_;
  std::unique_ptr<SyncChannel> non_restricted_channel_;
  mojo::ScopedMessagePipeHandle non_restricted_channel_handle_;
};

TEST_F(IPCSyncChannelTest, RestrictedDispatch) {
  WaitableEvent sent_ping_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent wait_event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  mojo::MessagePipe restricted_pipe, non_restricted_pipe;
  RestrictedDispatchServer* server = new RestrictedDispatchServer(
      &sent_ping_event, &wait_event, std::move(restricted_pipe.handle0));
  NonRestrictedDispatchServer* server2 = new NonRestrictedDispatchServer(
      &wait_event, std::move(non_restricted_pipe.handle0));

  int success = 0;
  std::vector<Worker*> workers;
  workers.push_back(server);
  workers.push_back(server2);
  workers.push_back(
      new RestrictedDispatchClient(&sent_ping_event, server, server2, &success,
                                   std::move(restricted_pipe.handle1),
                                   std::move(non_restricted_pipe.handle1)));
  RunTest(workers);
  EXPECT_EQ(4, success);
}

//------------------------------------------------------------------------------

// This test case inspired by crbug.com/108491
// We create two servers that use the same ListenerThread but have
// SetRestrictDispatchToSameChannel set to true.
// We create clients, then use some specific WaitableEvent wait/signalling to
// ensure that messages get dispatched in a way that causes a deadlock due to
// a nested dispatch and an eligible message in a higher-level dispatch's
// delayed_queue. Specifically, we start with client1 about so send an
// unblocking message to server1, while the shared listener thread for the
// servers server1 and server2 is about to send a non-unblocking message to
// client1. At the same time, client2 will be about to send an unblocking
// message to server2. Server1 will handle the client1->server1 message by
// telling server2 to send a non-unblocking message to client2.
// What should happen is that the send to server2 should find the pending,
// same-context client2->server2 message to dispatch, causing client2 to
// unblock then handle the server2->client2 message, so that the shared
// servers' listener thread can then respond to the client1->server1 message.
// Then client1 can handle the non-unblocking server1->client1 message.
// The old code would end up in a state where the server2->client2 message is
// sent, but the client2->server2 message (which is eligible for dispatch, and
// which is what client2 is waiting for) is stashed in a local delayed_queue
// that has server1's channel context, causing a deadlock.
// WaitableEvents in the events array are used to:
//   event 0: indicate to client1 that server listener is in OnDoServerTask
//   event 1: indicate to client1 that client2 listener is in OnDoClient2Task
//   event 2: indicate to server1 that client2 listener is in OnDoClient2Task
//   event 3: indicate to client2 that server listener is in OnDoServerTask

class RestrictedDispatchDeadlockServer : public Worker {
 public:
  RestrictedDispatchDeadlockServer(int server_num,
                                   WaitableEvent* server_ready_event,
                                   WaitableEvent** events,
                                   RestrictedDispatchDeadlockServer* peer,
                                   mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(std::move(channel_handle), Channel::MODE_SERVER),
        server_num_(server_num),
        server_ready_event_(server_ready_event),
        events_(events),
        peer_(peer) {}

  void OnDoServerTask() {
    events_[3]->Signal();
    events_[2]->Wait();
    events_[0]->Signal();
    SendMessageToClient();
  }

  void Run() override {
    channel()->SetRestrictDispatchChannelGroup(1);
    server_ready_event_->Signal();
  }

  base::Thread* ListenerThread() { return Worker::ListenerThread(); }

 private:
  bool OnMessageReceived(const Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(RestrictedDispatchDeadlockServer, message)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_NoArgs, OnNoArgs)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_Done, Done)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnNoArgs() {
    if (server_num_ == 1) {
      DCHECK(peer_);
      peer_->SendMessageToClient();
    }
  }

  void SendMessageToClient() {
    Message* msg = new SyncChannelTestMsg_NoArgs;
    msg->set_unblock(false);
    DCHECK(!msg->should_unblock());
    Send(msg);
  }

  int server_num_;
  raw_ptr<WaitableEvent> server_ready_event_;
  raw_ptr<WaitableEvent*, AllowPtrArithmetic> events_;
  raw_ptr<RestrictedDispatchDeadlockServer, DanglingUntriaged> peer_;
};

class RestrictedDispatchDeadlockClient2 : public Worker {
 public:
  RestrictedDispatchDeadlockClient2(
      RestrictedDispatchDeadlockServer* server,
      WaitableEvent* server_ready_event,
      WaitableEvent** events,
      mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(std::move(channel_handle), Channel::MODE_CLIENT),
        server_ready_event_(server_ready_event),
        events_(events),
        received_msg_(false),
        received_noarg_reply_(false),
        done_issued_(false) {}

  void Run() override {
    server_ready_event_->Wait();
  }

  void OnDoClient2Task() {
    events_[3]->Wait();
    events_[1]->Signal();
    events_[2]->Signal();
    DCHECK(received_msg_ == false);

    Message* message = new SyncChannelTestMsg_NoArgs;
    message->set_unblock(true);
    Send(message);
    received_noarg_reply_ = true;
  }

  base::Thread* ListenerThread() { return Worker::ListenerThread(); }

 private:
  bool OnMessageReceived(const Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(RestrictedDispatchDeadlockClient2, message)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_NoArgs, OnNoArgs)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnNoArgs() {
    received_msg_ = true;
    PossiblyDone();
  }

  void PossiblyDone() {
    if (received_noarg_reply_ && received_msg_) {
      DCHECK(done_issued_ == false);
      done_issued_ = true;
      Send(new SyncChannelTestMsg_Done);
      Done();
    }
  }

  raw_ptr<WaitableEvent> server_ready_event_;
  raw_ptr<WaitableEvent*, AllowPtrArithmetic> events_;
  bool received_msg_;
  bool received_noarg_reply_;
  bool done_issued_;
};

class RestrictedDispatchDeadlockClient1 : public Worker {
 public:
  RestrictedDispatchDeadlockClient1(
      RestrictedDispatchDeadlockServer* server,
      RestrictedDispatchDeadlockClient2* peer,
      WaitableEvent* server_ready_event,
      WaitableEvent** events,
      mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(std::move(channel_handle), Channel::MODE_CLIENT),
        server_(server),
        peer_(peer),
        server_ready_event_(server_ready_event),
        events_(events),
        received_msg_(false),
        received_noarg_reply_(false),
        done_issued_(false) {}

  void Run() override {
    server_ready_event_->Wait();
    server_->ListenerThread()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RestrictedDispatchDeadlockServer::OnDoServerTask,
                       base::Unretained(server_)));
    peer_->ListenerThread()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RestrictedDispatchDeadlockClient2::OnDoClient2Task,
                       base::Unretained(peer_)));
    events_[0]->Wait();
    events_[1]->Wait();
    DCHECK(received_msg_ == false);

    Message* message = new SyncChannelTestMsg_NoArgs;
    message->set_unblock(true);
    Send(message);
    received_noarg_reply_ = true;
    PossiblyDone();
  }

 private:
  bool OnMessageReceived(const Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(RestrictedDispatchDeadlockClient1, message)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_NoArgs, OnNoArgs)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnNoArgs() {
    received_msg_ = true;
    PossiblyDone();
  }

  void PossiblyDone() {
    if (received_noarg_reply_ && received_msg_) {
      DCHECK(done_issued_ == false);
      done_issued_ = true;
      Send(new SyncChannelTestMsg_Done);
      Done();
    }
  }

  raw_ptr<RestrictedDispatchDeadlockServer, DanglingUntriaged> server_;
  raw_ptr<RestrictedDispatchDeadlockClient2, DanglingUntriaged> peer_;
  raw_ptr<WaitableEvent> server_ready_event_;
  raw_ptr<WaitableEvent*, AllowPtrArithmetic> events_;
  bool received_msg_;
  bool received_noarg_reply_;
  bool done_issued_;
};

TEST_F(IPCSyncChannelTest, RestrictedDispatchDeadlock) {
  std::vector<Worker*> workers;

  // A shared worker thread so that server1 and server2 run on one thread.
  base::Thread worker_thread("RestrictedDispatchDeadlock");
  ASSERT_TRUE(worker_thread.Start());

  WaitableEvent server1_ready(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent server2_ready(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);

  WaitableEvent event0(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent event1(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent event2(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent event3(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent* events[4] = {&event0, &event1, &event2, &event3};

  RestrictedDispatchDeadlockServer* server1;
  RestrictedDispatchDeadlockServer* server2;
  RestrictedDispatchDeadlockClient1* client1;
  RestrictedDispatchDeadlockClient2* client2;

  mojo::MessagePipe pipe1, pipe2;
  server2 = new RestrictedDispatchDeadlockServer(
      2, &server2_ready, events, nullptr, std::move(pipe2.handle0));
  server2->OverrideThread(&worker_thread);
  workers.push_back(server2);

  client2 = new RestrictedDispatchDeadlockClient2(
      server2, &server2_ready, events, std::move(pipe2.handle1));
  workers.push_back(client2);

  server1 = new RestrictedDispatchDeadlockServer(
      1, &server1_ready, events, server2, std::move(pipe1.handle0));
  server1->OverrideThread(&worker_thread);
  workers.push_back(server1);

  client1 = new RestrictedDispatchDeadlockClient1(
      server1, client2, &server1_ready, events, std::move(pipe1.handle1));
  workers.push_back(client1);

  RunTest(workers);
}

//------------------------------------------------------------------------------

// This test case inspired by crbug.com/120530
// We create 4 workers that pipe to each other W1->W2->W3->W4->W1 then we send a
// message that recurses through 3, 4 or 5 steps to make sure, say, W1 can
// re-enter when called from W4 while it's sending a message to W2.
// The first worker drives the whole test so it must be treated specially.

class RestrictedDispatchPipeWorker : public Worker {
 public:
  RestrictedDispatchPipeWorker(mojo::ScopedMessagePipeHandle channel_handle1,
                               WaitableEvent* event1,
                               mojo::ScopedMessagePipeHandle channel_handle2,
                               WaitableEvent* event2,
                               int group,
                               int* success)
      : Worker(std::move(channel_handle1), Channel::MODE_SERVER),
        event1_(event1),
        event2_(event2),
        other_channel_handle_(std::move(channel_handle2)),
        group_(group),
        success_(success) {}

  void OnPingTTL(int ping, int* ret) {
    *ret = 0;
    if (!ping)
      return;
    other_channel_->Send(new SyncChannelTestMsg_PingTTL(ping - 1, ret));
    ++*ret;
  }

  void OnDone() {
    if (is_first())
      return;
    other_channel_->Send(new SyncChannelTestMsg_Done);
    other_channel_.reset();
    Done();
  }

  void Run() override {
    channel()->SetRestrictDispatchChannelGroup(group_);
    if (is_first())
      event1_->Signal();
    event2_->Wait();
    other_channel_ = SyncChannel::Create(
        other_channel_handle_.release(), IPC::Channel::MODE_CLIENT, this,
        ipc_thread().task_runner(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), true,
        shutdown_event());
    other_channel_->SetRestrictDispatchChannelGroup(group_);
    if (!is_first()) {
      event1_->Signal();
      return;
    }
    *success_ = 0;
    int value = 0;
    OnPingTTL(3, &value);
    *success_ += (value == 3);
    OnPingTTL(4, &value);
    *success_ += (value == 4);
    OnPingTTL(5, &value);
    *success_ += (value == 5);
    other_channel_->Send(new SyncChannelTestMsg_Done);
    other_channel_.reset();
    Done();
  }

  bool is_first() { return !!success_; }

 private:
  bool OnMessageReceived(const Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(RestrictedDispatchPipeWorker, message)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_PingTTL, OnPingTTL)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_Done, OnDone)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  std::unique_ptr<SyncChannel> other_channel_;
  raw_ptr<WaitableEvent> event1_;
  raw_ptr<WaitableEvent> event2_;
  mojo::ScopedMessagePipeHandle other_channel_handle_;
  int group_;
  raw_ptr<int> success_;
};

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_RestrictedDispatch4WayDeadlock \
  DISABLED_RestrictedDispatch4WayDeadlock
#else
#define MAYBE_RestrictedDispatch4WayDeadlock RestrictedDispatch4WayDeadlock
#endif
TEST_F(IPCSyncChannelTest, MAYBE_RestrictedDispatch4WayDeadlock) {
  int success = 0;
  std::vector<Worker*> workers;
  WaitableEvent event0(base::WaitableEvent::ResetPolicy::MANUAL,
                       base::WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent event1(base::WaitableEvent::ResetPolicy::MANUAL,
                       base::WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent event2(base::WaitableEvent::ResetPolicy::MANUAL,
                       base::WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent event3(base::WaitableEvent::ResetPolicy::MANUAL,
                       base::WaitableEvent::InitialState::NOT_SIGNALED);
  mojo::MessagePipe pipe0, pipe1, pipe2, pipe3;
  workers.push_back(new RestrictedDispatchPipeWorker(
      std::move(pipe0.handle0), &event0, std::move(pipe1.handle1), &event1, 1,
      &success));
  workers.push_back(new RestrictedDispatchPipeWorker(
      std::move(pipe1.handle0), &event1, std::move(pipe2.handle1), &event2, 2,
      nullptr));
  workers.push_back(new RestrictedDispatchPipeWorker(
      std::move(pipe2.handle0), &event2, std::move(pipe3.handle1), &event3, 3,
      nullptr));
  workers.push_back(new RestrictedDispatchPipeWorker(
      std::move(pipe3.handle0), &event3, std::move(pipe0.handle1), &event0, 4,
      nullptr));
  RunTest(workers);
  EXPECT_EQ(3, success);
}

//------------------------------------------------------------------------------

// This test case inspired by crbug.com/122443
// We want to make sure a reply message with the unblock flag set correctly
// behaves as a reply, not a regular message.
// We have 3 workers. Server1 will send a message to Server2 (which will block),
// during which it will dispatch a message comming from Client, at which point
// it will send another message to Server2. While sending that second message it
// will receive a reply from Server1 with the unblock flag.

class ReentrantReplyServer1 : public Worker {
 public:
  ReentrantReplyServer1(WaitableEvent* server_ready,
                        mojo::ScopedMessagePipeHandle channel_handle1,
                        mojo::ScopedMessagePipeHandle channel_handle2)
      : Worker(std::move(channel_handle1), Channel::MODE_SERVER),
        server_ready_(server_ready),
        other_channel_handle_(std::move(channel_handle2)) {}

  void Run() override {
    server2_channel_ = SyncChannel::Create(
        other_channel_handle_.release(), IPC::Channel::MODE_CLIENT, this,
        ipc_thread().task_runner(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), true,
        shutdown_event());
    server_ready_->Signal();
    Message* msg = new SyncChannelTestMsg_Reentrant1();
    server2_channel_->Send(msg);
    server2_channel_.reset();
    Done();
  }

 private:
  bool OnMessageReceived(const Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(ReentrantReplyServer1, message)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_Reentrant2, OnReentrant2)
     IPC_REPLY_HANDLER(OnReply)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnReentrant2() {
    Message* msg = new SyncChannelTestMsg_Reentrant3();
    server2_channel_->Send(msg);
  }

  void OnReply(const Message& message) {
    // If we get here, the Send() will never receive the reply (thus would
    // hang), so abort instead.
    LOG(FATAL) << "Reply message was dispatched";
  }

  raw_ptr<WaitableEvent> server_ready_;
  std::unique_ptr<SyncChannel> server2_channel_;
  mojo::ScopedMessagePipeHandle other_channel_handle_;
};

class ReentrantReplyServer2 : public Worker {
 public:
  ReentrantReplyServer2(mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(std::move(channel_handle), Channel::MODE_SERVER),
        reply_(nullptr) {}

 private:
  bool OnMessageReceived(const Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(ReentrantReplyServer2, message)
     IPC_MESSAGE_HANDLER_DELAY_REPLY(
         SyncChannelTestMsg_Reentrant1, OnReentrant1)
     IPC_MESSAGE_HANDLER(SyncChannelTestMsg_Reentrant3, OnReentrant3)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void OnReentrant1(Message* reply) {
    DCHECK(!reply_);
    reply_ = reply;
  }

  void OnReentrant3() {
    DCHECK(reply_);
    Message* reply = reply_;
    reply_ = nullptr;
    reply->set_unblock(true);
    Send(reply);
    Done();
  }

  raw_ptr<Message> reply_;
};

class ReentrantReplyClient : public Worker {
 public:
  ReentrantReplyClient(WaitableEvent* server_ready,
                       mojo::ScopedMessagePipeHandle channel_handle)
      : Worker(std::move(channel_handle), Channel::MODE_CLIENT),
        server_ready_(server_ready) {}

  void Run() override {
    server_ready_->Wait();
    Send(new SyncChannelTestMsg_Reentrant2());
    Done();
  }

 private:
  raw_ptr<WaitableEvent> server_ready_;
};

TEST_F(IPCSyncChannelTest, ReentrantReply) {
  std::vector<Worker*> workers;
  WaitableEvent server_ready(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  mojo::MessagePipe pipe1, pipe2;
  workers.push_back(new ReentrantReplyServer2(std::move(pipe2.handle0)));
  workers.push_back(new ReentrantReplyServer1(
      &server_ready, std::move(pipe1.handle0), std::move(pipe2.handle1)));
  workers.push_back(
      new ReentrantReplyClient(&server_ready, std::move(pipe1.handle1)));
  RunTest(workers);
}

}  // namespace
}  // namespace IPC
