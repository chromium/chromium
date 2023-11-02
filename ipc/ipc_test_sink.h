// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_TEST_SINK_H_
#define IPC_IPC_TEST_SINK_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/observer_list.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"

namespace IPC {

class Message;

// This test sink provides a "sink" for IPC messages that are sent. It allows
// the caller to query messages received in various different ways.  It is
// designed for tests for objects that use the IPC system.
//
// Typical usage:
//
//   test_sink.ClearMessages();
//   do_something();
//
//   // We should have gotten exactly one update state message.
//   EXPECT_TRUE(test_sink.GetUniqeMessageMatching(ViewHostMsg_Update::ID));
//   // ...and no start load messages.
//   EXPECT_FALSE(test_sink.GetFirstMessageMatching(ViewHostMsg_Start::ID));
//
//   // Now inspect a message. This assumes a message that was declared like
//   // this: IPC_MESSAGE_ROUTED2(ViewMsg_Foo, bool, int)
//   IPC::Message* msg = test_sink.GetFirstMessageMatching(ViewMsg_Foo::ID));
//   ASSERT_TRUE(msg);
//   bool first_param;
//   int second_param;
//   ViewMsg_Foo::Read(msg, &first_param, &second_param);
//
//   // Go on to the next phase of the test.
//   test_sink.ClearMessages();
//
// To read a sync reply, do this:
//
//   IPC::Message* msg = test_sink.GetUniqueMessageMatching(IPC_REPLY_ID);
//   ASSERT_TRUE(msg);
//   base::TupleTypes<ViewHostMsg_Foo::ReplyParam>::ValueTuple reply_data;
//   EXPECT_TRUE(ViewHostMsg_Foo::ReadReplyParam(msg, &reply_data));
//
// You can also register to be notified when messages are posted to the sink.
// This can be useful if you need to wait for a particular message that will
// be posted asynchronously.  Example usage:
//
//   class MyListener : public IPC::Listener {
//    public:
//     MyListener(const base::RepeatingClosure& closure)
//       : message_received_closure_(closure) {}
//     virtual bool OnMessageReceived(const IPC::Message& msg) {
//       <do something with the message>
//       message_received_closure_.Run();
//       return false;  // to store the message in the sink, or true to drop it
//     }
//    private:
//     base::RepeatingClosure message_received_closure_;
//   };
//
//   base::RunLoop run_loop;
//   MyListener listener(run_loop.QuitClosure());
//   test_sink.AddFilter(&listener);
//   StartSomeAsynchronousProcess(&test_sink);
//   run_loop.Run();
//   <inspect the results>
//   ...
//
// To hook up the sink, all you need to do is call OnMessageReceived when a
// message is received.
class TestSink : public Channel {
 public:
  TestSink();

  TestSink(const TestSink&) = delete;
  TestSink& operator=(const TestSink&) = delete;

  ~TestSink() override;

  // Interface in IPC::Channel. This copies the message to the sink and then
  // deletes it.
  bool Send(IPC::Message* message) override;
  [[nodiscard]] bool Connect() override;
  void Close() override;

  // Used by the source of the messages to send the message to the sink. This
  // will make a copy of the message and store it in the list.
  bool OnMessageReceived(const Message& msg);

  // Returns the number of messages in the queue.
  size_t message_count() const { return messages_.size(); }

  // Clears the message queue of saved messages.
  void ClearMessages();

  // Returns the message at the given index in the queue. The index may be out
  // of range, in which case the return value is NULL. The returned pointer will
  // only be valid until another message is received or the list is cleared.
  const Message* GetMessageAt(size_t index) const;

  // Returns the first message with the given ID in the queue. If there is no
  // message with the given ID, returns NULL. The returned pointer will only be
  // valid until another message is received or the list is cleared.
  const Message* GetFirstMessageMatching(uint32_t id) const;

  // Returns the message with the given ID in the queue. If there is no such
  // message or there is more than one of that message, this will return NULL
  // (with the expectation that you'll do an ASSERT_TRUE() on the result).
  // The returned pointer will only be valid until another message is received
  // or the list is cleared.
  const Message* GetUniqueMessageMatching(uint32_t id) const;

  // Adds the given listener as a filter to the TestSink.
  // When a message is received by the TestSink, it will be dispatched to
  // the filters, in the order they were added.  If a filter returns true
  // from OnMessageReceived, subsequent filters will not receive the message
  // and the TestSink will not store it.
  void AddFilter(Listener* filter);

  // Removes the given filter from the TestSink.
  void RemoveFilter(Listener* filter);

 private:
  // The actual list of received messages.
  std::vector<Message> messages_;
  base::ObserverList<Listener>::Unchecked filter_list_;
};

}  // namespace IPC

#endif  // IPC_IPC_TEST_SINK_H_
