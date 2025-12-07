// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGING_PIPE_MESSAGING_CHANNEL_H_
#define REMOTING_HOST_NATIVE_MESSAGING_PIPE_MESSAGING_CHANNEL_H_

#include <memory>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "extensions/browser/api/messaging/native_messaging_channel.h"
#include "remoting/host/native_messaging/native_messaging_reader.h"
#include "remoting/host/native_messaging/native_messaging_writer.h"

namespace remoting {

// An implementation of extensions::NativeMessagingChannel using a pipe. It
// is used by the It2MeNativeMessagingHost and Me2MeNativeMessagingHost to
// communicate with the chrome process.
// TODO(kelvinp): Move this class to the extensions/browser/api/messaging
// directory.
class PipeMessagingChannel : public extensions::NativeMessagingChannel {
 public:
  typedef extensions::NativeMessagingChannel::EventHandler EventHandler;

  // Constructs an object taking the ownership of |input| and |output|.
  PipeMessagingChannel(base::File input, base::File output);

  PipeMessagingChannel(const PipeMessagingChannel&) = delete;
  PipeMessagingChannel& operator=(const PipeMessagingChannel&) = delete;

  ~PipeMessagingChannel() override;

#if BUILDFLAG(IS_POSIX)
  // Opens `stdin_file` and `stdout_file` with stdin and stdout respectively,
  // which will NOT be assigned file descriptors 0 (STDIN_FILENO) and 1
  // (STDOUT_FILENO), then points file descriptors 0 and 1 to /dev/null, so that
  // child processes can't inherit stdin and stdout from this process and
  // potentially corrupt the native messaging stream.
  static void OpenAndBlockStdio(base::File& stdin_file,
                                base::File& stdout_file);
#endif  // BUILDFLAG(IS_POSIX)

  // extensions::NativeMessagingChannel implementation.
  void Start(EventHandler* event_handler) override;
  void SendMessage(std::optional<base::ValueView> message) override;

 private:
  // Processes a message received from the client app.
  void ProcessMessage(base::Value message);

  // Initiates shutdown.
  void Shutdown();

  NativeMessagingReader native_messaging_reader_;
  std::unique_ptr<NativeMessagingWriter> native_messaging_writer_;

  raw_ptr<EventHandler> event_handler_;
  base::WeakPtr<PipeMessagingChannel> weak_ptr_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PipeMessagingChannel> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_NATIVE_MESSAGING_PIPE_MESSAGING_CHANNEL_H_
