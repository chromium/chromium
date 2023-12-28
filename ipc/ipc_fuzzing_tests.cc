// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>

#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "ipc/ipc_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

// IPC messages for testing ----------------------------------------------------

#define IPC_MESSAGE_IMPL
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"

#define IPC_MESSAGE_START TestMsgStart

// Generic message class that is an int followed by a string16.
IPC_MESSAGE_CONTROL2(MsgClassIS, int, std::u16string)

// Generic message class that is a string16 followed by an int.
IPC_MESSAGE_CONTROL2(MsgClassSI, std::u16string, int)

// Message to create a mutex in the IPC server, using the received name.
IPC_MESSAGE_CONTROL2(MsgDoMutex, std::u16string, int)

// Used to generate an ID for a message that should not exist.
IPC_MESSAGE_CONTROL0(MsgUnhandled)

// -----------------------------------------------------------------------------

namespace {

TEST(IPCMessageIntegrity, ReadBeyondBufferStr) {
  // This was BUG 984408.
  uint32_t v1 = std::numeric_limits<uint32_t>::max() - 1;
  int v2 = 666;
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(v1);
  m.WriteInt(v2);

  base::PickleIterator iter(m);
  std::string vs;
  EXPECT_FALSE(iter.ReadString(&vs));
}

TEST(IPCMessageIntegrity, ReadBeyondBufferStr16) {
  // This was BUG 984408.
  uint32_t v1 = std::numeric_limits<uint32_t>::max() - 1;
  int v2 = 777;
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(v1);
  m.WriteInt(v2);

  base::PickleIterator iter(m);
  std::u16string vs;
  EXPECT_FALSE(iter.ReadString16(&vs));
}

TEST(IPCMessageIntegrity, ReadBytesBadIterator) {
  // This was BUG 1035467.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(1);
  m.WriteInt(2);

  base::PickleIterator iter(m);
  const char* data = nullptr;
  EXPECT_TRUE(iter.ReadBytes(&data, sizeof(int)));
}

TEST(IPCMessageIntegrity, ReadVectorNegativeSize) {
  // A slight variation of BUG 984408. Note that the pickling of vector<char>
  // has a specialized template which is not vulnerable to this bug. So here
  // try to hit the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(-1);  // This is the count of elements.
  m.WriteInt(1);
  m.WriteInt(2);
  m.WriteInt(3);

  std::vector<double> vec;
  base::PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ReadVectorTooLarge1 DISABLED_ReadVectorTooLarge1
#else
#define MAYBE_ReadVectorTooLarge1 ReadVectorTooLarge1
#endif
TEST(IPCMessageIntegrity, MAYBE_ReadVectorTooLarge1) {
  // This was BUG 1006367. This is the large but positive length case. Again
  // we try to hit the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(0x21000003);  // This is the count of elements.
  m.WriteInt64(1);
  m.WriteInt64(2);

  std::vector<int64_t> vec;
  base::PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

TEST(IPCMessageIntegrity, ReadVectorTooLarge2) {
  // This was BUG 1006367. This is the large but positive with an additional
  // integer overflow when computing the actual byte size. Again we try to hit
  // the non-specialized case vector<P>.
  IPC::Message m(0, 1, IPC::Message::PRIORITY_NORMAL);
  m.WriteInt(0x71000000);  // This is the count of elements.
  m.WriteInt64(1);
  m.WriteInt64(2);

  std::vector<int64_t> vec;
  base::PickleIterator iter(m);
  EXPECT_FALSE(ReadParam(&m, &iter, &vec));
}

// This test needs ~20 seconds in Debug mode, or ~4 seconds in Release mode.
// See http://crbug.com/741866 for details.
TEST(IPCMessageIntegrity, DISABLED_ReadVectorTooLarge3) {
  base::Pickle pickle;
  IPC::WriteParam(&pickle, 256 * 1024 * 1024);
  IPC::WriteParam(&pickle, 0);
  IPC::WriteParam(&pickle, 1);
  IPC::WriteParam(&pickle, 2);

  base::PickleIterator iter(pickle);
  std::vector<int> vec;
  EXPECT_FALSE(IPC::ReadParam(&pickle, &iter, &vec));
}

class SimpleListener : public IPC::Listener {
 public:
  SimpleListener() = default;
  void Init(IPC::Sender* s) { other_ = s; }
  void set_run_loop(base::RunLoop* loop) { loop_ = loop; }
  void Reset() {
    other_ = nullptr;
    loop_ = nullptr;
  }

 protected:
  raw_ptr<base::RunLoop> loop_ = nullptr;
  raw_ptr<IPC::Sender> other_ = nullptr;
};

enum {
  FUZZER_ROUTING_ID = 5
};

// The fuzzer server class. It runs in a child process and expects
// only two IPC calls; after that it exits the message loop which
// terminates the child process.
class FuzzerServerListener : public SimpleListener {
 public:
  FuzzerServerListener() : message_count_(2), pending_messages_(0) {
  }
  bool OnMessageReceived(const IPC::Message& msg) override {
    if (msg.routing_id() == MSG_ROUTING_CONTROL) {
      ++pending_messages_;
      IPC_BEGIN_MESSAGE_MAP(FuzzerServerListener, msg)
        IPC_MESSAGE_HANDLER(MsgClassIS, OnMsgClassISMessage)
        IPC_MESSAGE_HANDLER(MsgClassSI, OnMsgClassSIMessage)
      IPC_END_MESSAGE_MAP()
      if (pending_messages_) {
        // Probably a problem de-serializing the message.
        ReplyMsgNotHandled(msg.type());
      }
    }
    return true;
  }

 private:
  void OnMsgClassISMessage(int value, const std::u16string& text) {
    UseData(MsgClassIS::ID, value, text);
    RoundtripAckReply(FUZZER_ROUTING_ID, MsgClassIS::ID, value);
    Cleanup();
  }

  void OnMsgClassSIMessage(const std::u16string& text, int value) {
    UseData(MsgClassSI::ID, value, text);
    RoundtripAckReply(FUZZER_ROUTING_ID, MsgClassSI::ID, value);
    Cleanup();
  }

  bool RoundtripAckReply(int routing, uint32_t type_id, int reply) {
    IPC::Message* message = new IPC::Message(routing, type_id,
                                             IPC::Message::PRIORITY_NORMAL);
    message->WriteInt(reply + 1);
    message->WriteInt(reply);
    return other_->Send(message);
  }

  void Cleanup() {
    --message_count_;
    --pending_messages_;
    if (0 == message_count_)
      loop_->QuitWhenIdle();
  }

  void ReplyMsgNotHandled(uint32_t type_id) {
    RoundtripAckReply(FUZZER_ROUTING_ID, MsgUnhandled::ID, type_id);
    Cleanup();
  }

  void UseData(int caller, int value, const std::u16string& text) {
    std::ostringstream os;
    os << "IPC fuzzer:" << caller << " [" << value << " "
       << base::UTF16ToUTF8(text) << "]\n";
    std::string output = os.str();
    LOG(WARNING) << output;
  }

  int message_count_;
  int pending_messages_;
};

class FuzzerClientListener : public SimpleListener {
 public:
  FuzzerClientListener() = default;

  bool OnMessageReceived(const IPC::Message& msg) override {
    last_msg_ = std::make_unique<IPC::Message>(msg);
    loop_->QuitWhenIdle();
    return true;
  }

  bool ExpectMessage(int value, uint32_t type_id) {
    if (!MsgHandlerInternal(type_id))
      return false;
    int msg_value1 = 0;
    int msg_value2 = 0;
    base::PickleIterator iter(*last_msg_);
    if (!iter.ReadInt(&msg_value1))
      return false;
    if (!iter.ReadInt(&msg_value2))
      return false;
    if ((msg_value2 + 1) != msg_value1)
      return false;
    if (msg_value2 != value)
      return false;
    last_msg_.reset();
    return true;
  }

  bool ExpectMsgNotHandled(uint32_t type_id) {
    return ExpectMessage(type_id, MsgUnhandled::ID);
  }

 private:
  bool MsgHandlerInternal(uint32_t type_id) {
    loop_->Run();
    if (!last_msg_)
      return false;
    if (FUZZER_ROUTING_ID != last_msg_->routing_id())
      return false;
    return (type_id == last_msg_->type());
  }

  std::unique_ptr<IPC::Message> last_msg_;
};

// Runs the fuzzing server child mode. Returns when the preset number of
// messages have been received.
DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(FuzzServerClient) {
  FuzzerServerListener listener;
  base::RunLoop loop;
  Connect(&listener);
  listener.Init(channel());
  listener.set_run_loop(&loop);
  loop.Run();
  Close();
}

using IPCFuzzingTest = IPCChannelMojoTestBase;

// This test makes sure that the FuzzerClientListener and FuzzerServerListener
// are working properly by generating two well formed IPC calls.
TEST_F(IPCFuzzingTest, SanityTest) {
  Init("FuzzServerClient");
  base::RunLoop loop1;
  base::RunLoop loop2;
  FuzzerClientListener listener;
  CreateChannel(&listener);
  listener.Init(channel());
  listener.set_run_loop(&loop1);
  ASSERT_TRUE(ConnectChannel());

  IPC::Message* msg = nullptr;
  int value = 43;
  msg = new MsgClassIS(value, u"expect 43");
  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(value, MsgClassIS::ID));

  listener.set_run_loop(&loop2);
  msg = new MsgClassSI(u"expect 44", ++value);
  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(value, MsgClassSI::ID));

  listener.Reset();
  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

// This test uses a payload that is smaller than expected. This generates an
// error while unpacking the IPC buffer. Right after we generate another valid
// IPC to make sure framing is working properly.
TEST_F(IPCFuzzingTest, MsgBadPayloadShort) {
  Init("FuzzServerClient");
  base::RunLoop loop1;
  base::RunLoop loop2;
  FuzzerClientListener listener;
  CreateChannel(&listener);
  listener.Init(channel());
  listener.set_run_loop(&loop1);
  ASSERT_TRUE(ConnectChannel());

  IPC::Message* msg = new IPC::Message(MSG_ROUTING_CONTROL, MsgClassIS::ID,
                                       IPC::Message::PRIORITY_NORMAL);
  msg->WriteInt(666);
  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMsgNotHandled(MsgClassIS::ID));

  listener.set_run_loop(&loop2);
  msg = new MsgClassSI(u"expect one", 1);
  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(1, MsgClassSI::ID));

  listener.Reset();
  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

// This test uses a payload that has too many arguments, but so the payload size
// is big enough so the unpacking routine does not generate an error as in the
// case of MsgBadPayloadShort test. This test does not pinpoint a flaw (per se)
// as by design we don't carry type information on the IPC message.
TEST_F(IPCFuzzingTest, MsgBadPayloadArgs) {
  Init("FuzzServerClient");
  base::RunLoop loop1;
  base::RunLoop loop2;
  FuzzerClientListener listener;
  CreateChannel(&listener);
  listener.Init(channel());
  listener.set_run_loop(&loop1);
  ASSERT_TRUE(ConnectChannel());

  IPC::Message* msg = new IPC::Message(MSG_ROUTING_CONTROL, MsgClassSI::ID,
                                       IPC::Message::PRIORITY_NORMAL);
  msg->WriteString16(u"d");
  msg->WriteInt(0);
  msg->WriteInt(0x65);  // Extra argument.

  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(0, MsgClassSI::ID));

  listener.set_run_loop(&loop2);
  // Now send a well formed message to make sure the receiver wasn't
  // thrown out of sync by the extra argument.
  msg = new MsgClassIS(3, u"expect three");
  sender()->Send(msg);
  EXPECT_TRUE(listener.ExpectMessage(3, MsgClassIS::ID));

  listener.Reset();
  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

}  // namespace
