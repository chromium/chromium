// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_SECURITY_TEST_UTIL_H_
#define IPC_IPC_SECURITY_TEST_UTIL_H_

namespace IPC {

class ChannelProxy;
class Message;

class IpcSecurityTestUtil {
 public:
  IpcSecurityTestUtil(const IpcSecurityTestUtil&) = delete;
  IpcSecurityTestUtil& operator=(const IpcSecurityTestUtil&) = delete;

  // Enables testing of security exploit scenarios where a compromised child
  // process can send a malicious message of an arbitrary type.
  //
  // This function will post the message to the IPC channel's thread, where it
  // is offered to the channel's listeners. Afterwards, a reply task is posted
  // back to the current thread. This function blocks until the reply task is
  // received. For messages forwarded back to the current thread, we won't
  // return until after the message has been handled here.
  //
  // Use this only for testing security bugs in a browsertest; other uses are
  // likely perilous. Unit tests should be using IPC::TestSink which has an
  // OnMessageReceived method you can call directly. Non-security browsertests
  // should just exercise the child process's normal codepaths to send messages.
  static void PwnMessageReceived(ChannelProxy* channel, const Message& message);

 private:
  IpcSecurityTestUtil();  // Not instantiable.
};

}  // namespace IPC

#endif  // IPC_IPC_SECURITY_TEST_UTIL_H_
