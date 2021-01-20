// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGING_PIPE_MESSAGING_CHANNEL_H_
#define REMOTING_HOST_NATIVE_MESSAGING_PIPE_MESSAGING_CHANNEL_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "extensions/browser/api/messaging/native_messaging_channel.h"
#include "remoting/host/native_messaging/native_messaging_reader.h"
#include "remoting/host/native_messaging/native_messaging_writer.h"

namespace base {
class Value;
}  // namespace base

namespace remoting {

// An implementation of extensions::NativeMessagingChannel using a pipe. It
// is used by the It2MeNativeMessagingHost and Me2MeNativeMessagingHost to
// communicate with the chrome process.
// TODO(kelvinp): Move this class to the extensions/browser/api/messaging
// directory.
class PipeMessagingChannel : public extensions::NativeMessagingChannel {
 public:
  typedef extensions::NativeMessagingChannel::EventHandler EventHandler;

  // Constructs an object taking the ownership of |input| and |output|. Closes
  // |input| and |output| to prevent the caller from using them.
  PipeMessagingChannel(base::File input, base::File output);
  ~PipeMessagingChannel() override;

  // If the ctor is called with |input| and |output| set to stdin/stdout,
  // it will close those file-descriptors. In that case, this helper function
  // should be used to recreate stdin/stdout as open files. This is needed on
  // POSIX because a later call to open() will return the lowest available
  // descriptors, and stdin or stdout could end up pointing at some random file,
  // which could cause an issue when, say, launching a child process.
  // This is POSIX-only (a no-op on other platforms) and is thread-unsafe, as it
  // calls open() twice, expecting it to return 0 then 1.
  static void ReopenStdinStdout();

  // extensions::NativeMessagingChannel implementation.
  void Start(EventHandler* event_handler) override;
  void SendMessage(std::unique_ptr<base::Value> message) override;

 private:
  // Processes a message received from the client app.
  void ProcessMessage(std::unique_ptr<base::Value> message);

  // Initiates shutdown.
  void Shutdown();

  NativeMessagingReader native_messaging_reader_;
  std::unique_ptr<NativeMessagingWriter> native_messaging_writer_;

  EventHandler* event_handler_;
  base::WeakPtr<PipeMessagingChannel> weak_ptr_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PipeMessagingChannel> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PipeMessagingChannel);
};

}  // namespace remoting

#endif  // REMOTING_HOST_NATIVE_MESSAGING_PIPE_MESSAGING_CHANNEL_H_
