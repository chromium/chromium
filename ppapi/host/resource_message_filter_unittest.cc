// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/host/resource_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ppapi {
namespace host {
namespace {

base::WaitableEvent g_handler_completion(
    base::WaitableEvent::ResetPolicy::MANUAL,
    base::WaitableEvent::InitialState::NOT_SIGNALED);

enum TestMessageTypes {
  MSG1_TYPE = 1,
  MSG2_TYPE,
  MSG3_TYPE,
  REPLY_MSG1_TYPE,
  REPLY_MSG2_TYPE,
  REPLY_MSG3_TYPE,
};

// Dummy resource host which simply stores a copy of messages it handles.
// |SendReply| is overridden to store a copy of the outgoing message and the
// message loop on which it was sent.
class MyResourceHost : public ResourceHost {
 public:
  // Messages of type |msg_type| will be handled (simply by replying with a
  // message of type |reply_msg_type|).
  MyResourceHost(PpapiHost* host,
                 PP_Instance instance,
                 PP_Resource resource,
                 uint32_t msg_type,
                 uint32_t reply_msg_type)
      : ResourceHost(host, instance, resource),
        msg_type_(msg_type),
        reply_msg_type_(reply_msg_type),
        last_reply_task_runner_(nullptr) {}

  const IPC::Message& last_handled_msg() const { return last_handled_msg_; }
  const IPC::Message& last_reply_msg() const { return last_reply_msg_; }
  scoped_refptr<base::SingleThreadTaskRunner> last_reply_task_runner() const {
    return last_reply_task_runner_;
  }

  void AddMessageFilter(scoped_refptr<ResourceMessageFilter> filter) {
    AddFilter(filter);
  }

  int32_t OnResourceMessageReceived(const IPC::Message& msg,
                                    HostMessageContext* context) override {
    last_handled_msg_ = msg;
    if (msg.type() == msg_type_) {
      context->reply_msg = IPC::Message(0, reply_msg_type_,
                                        IPC::Message::PRIORITY_NORMAL);
      return PP_OK;
    }
    return PP_ERROR_FAILED;
  }

  void SendReply(const ReplyMessageContext& context,
                 const IPC::Message& msg) override {
    last_reply_msg_ = msg;
    last_reply_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    g_handler_completion.Signal();
  }

 private:
  uint32_t msg_type_;
  uint32_t reply_msg_type_;

  IPC::Message last_handled_msg_;
  IPC::Message last_reply_msg_;
  scoped_refptr<base::SingleThreadTaskRunner> last_reply_task_runner_;
};

// Dummy message filter which simply stores a copy of messages it handles.
// The message loop on which the message is handled is also stored for checking
// later.
class MyResourceFilter : public ResourceMessageFilter {
 public:
  // Messages of type |msg_type| will be handled (simply by replying with a
  // message of type |reply_msg_type|). |io_thread| is the thread on which
  // replies should be sent. |bg_thread| is the thread on which the message
  // should be handled.
  MyResourceFilter(const base::Thread& io_thread,
                   const base::Thread& bg_thread,
                   uint32_t msg_type,
                   uint32_t reply_msg_type)
      : ResourceMessageFilter(io_thread.task_runner()),
        task_runner_(bg_thread.task_runner()),
        msg_type_(msg_type),
        reply_msg_type_(reply_msg_type) {}

  const IPC::Message& last_handled_msg() const { return last_handled_msg_; }
  scoped_refptr<base::SingleThreadTaskRunner> last_task_runner() const {
    return last_task_runner_;
  }

  scoped_refptr<base::SequencedTaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& msg) override {
    if (msg.type() == msg_type_)
      return task_runner_;
    return nullptr;
  }

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      HostMessageContext* context) override {
    last_handled_msg_ = msg;
    last_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    if (msg.type() == msg_type_) {
      context->reply_msg = IPC::Message(0, reply_msg_type_,
                                        IPC::Message::PRIORITY_NORMAL);
      return PP_OK;
    }
    return PP_ERROR_FAILED;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  uint32_t msg_type_;
  uint32_t reply_msg_type_;

  IPC::Message last_handled_msg_;
  scoped_refptr<base::SingleThreadTaskRunner> last_task_runner_;
};

}  // namespace

class ResourceMessageFilterTest : public testing::Test {
 public:
  void TestHandleMessageImpl() {
    base::Thread io_thread("test_io_thread");
    ASSERT_TRUE(io_thread.Start());

    base::Thread bg_thread1("test_background_thread1");
    ASSERT_TRUE(bg_thread1.Start());
    scoped_refptr<MyResourceFilter> filter1 =
        new MyResourceFilter(io_thread, bg_thread1, MSG1_TYPE, REPLY_MSG1_TYPE);

    base::Thread bg_thread2("test_background_thread2");
    ASSERT_TRUE(bg_thread2.Start());
    scoped_refptr<MyResourceFilter> filter2 =
        new MyResourceFilter(io_thread, bg_thread2, MSG2_TYPE, REPLY_MSG2_TYPE);

    PP_Instance instance = 12345;
    PP_Resource resource = 67890;
    MyResourceHost host(nullptr, instance, resource, MSG3_TYPE,
                        REPLY_MSG3_TYPE);
    host.AddMessageFilter(filter1);
    host.AddMessageFilter(filter2);

    proxy::ResourceMessageCallParams params(resource, 1);
    params.set_has_callback();
    HostMessageContext context(params);
    IPC::Message message1(0, MSG1_TYPE, IPC::Message::PRIORITY_NORMAL);
    IPC::Message message2(0, MSG2_TYPE, IPC::Message::PRIORITY_NORMAL);
    IPC::Message message3(0, MSG3_TYPE, IPC::Message::PRIORITY_NORMAL);

    // Message 1 handled by the first filter.
    host.HandleMessage(message1, &context);
    g_handler_completion.Wait();
    EXPECT_EQ(filter1->last_handled_msg().type(), message1.type());
    EXPECT_EQ(filter1->last_task_runner(), bg_thread1.task_runner());
    EXPECT_EQ(host.last_reply_msg().type(),
              static_cast<uint32_t>(REPLY_MSG1_TYPE));
    EXPECT_EQ(host.last_reply_task_runner(), io_thread.task_runner());
    g_handler_completion.Reset();

    // Message 2 handled by the second filter.
    host.HandleMessage(message2, &context);
    g_handler_completion.Wait();
    EXPECT_EQ(filter2->last_handled_msg().type(), message2.type());
    EXPECT_EQ(filter2->last_task_runner(), bg_thread2.task_runner());
    EXPECT_EQ(host.last_reply_msg().type(),
              static_cast<uint32_t>(REPLY_MSG2_TYPE));
    EXPECT_EQ(host.last_reply_task_runner(), io_thread.task_runner());
    g_handler_completion.Reset();

    // Message 3 handled by the resource host.
    host.HandleMessage(message3, &context);
    EXPECT_EQ(host.last_handled_msg().type(), message3.type());
    EXPECT_EQ(host.last_reply_msg().type(),
              static_cast<uint32_t>(REPLY_MSG3_TYPE));

    io_thread.Stop();
    bg_thread1.Stop();
    bg_thread2.Stop();
  }
};

// Test that messages are filtered correctly and handlers are run on the correct
// threads.
TEST_F(ResourceMessageFilterTest, TestHandleMessage) {
  // ResourceMessageFilter instances need to be created on a thread with message
  // loop. Therefore, we create a message loop and run the testing logic as a
  // task on it.
  base::test::SingleThreadTaskEnvironment task_environment;

  // It should be safe to use base::Unretained() because the object won't be
  // destroyed before the task is run.
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ResourceMessageFilterTest::TestHandleMessageImpl,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
}

}  // namespace proxy
}  // namespace ppapi
