// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_WRITER_H_
#define REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_WRITER_H_

#include "base/files/file.h"
#include "base/values.h"

namespace remoting {

// This class is used for sending messages to the Native Messaging client
// webapp.
class NativeMessagingWriter {
 public:
  explicit NativeMessagingWriter(base::File file);

  NativeMessagingWriter(const NativeMessagingWriter&) = delete;
  NativeMessagingWriter& operator=(const NativeMessagingWriter&) = delete;

  ~NativeMessagingWriter();

  // Sends a message to the Native Messaging client, returning true if
  // successful.
  bool WriteMessage(base::ValueView message);

 private:
  base::File write_stream_;
  bool fail_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_WRITER_H_
