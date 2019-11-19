// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_READER_H_
#define REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_READER_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"

namespace base {
class SequencedTaskRunner;
class Value;
}  // namespace base

namespace remoting {

// This class is used for reading messages from the Native Messaging client
// webapp.
class NativeMessagingReader {
 public:
  typedef base::Callback<void(std::unique_ptr<base::Value>)> MessageCallback;

  explicit NativeMessagingReader(base::File file);
  ~NativeMessagingReader();

  // Begin reading messages from the Native Messaging client webapp, calling
  // |message_callback| for each received message, or |eof_callback| if
  // EOF or error is encountered. This method is asynchronous - the callbacks
  // will be run on the same thread via PostTask. The caller should be prepared
  // for these callbacks to be invoked right up until this object is destroyed.
  void Start(MessageCallback message_callback, base::Closure eof_callback);

 private:
  class Core;
  friend class Core;

  // Wrappers posted to by the read thread to trigger the message and EOF
  // callbacks on the caller thread, and have them safely dropped if the reader
  // has been deleted before they are processed.
  void InvokeMessageCallback(std::unique_ptr<base::Value> message);
  void InvokeEofCallback();

  // Holds the information that the read thread needs to access, such as the
  // File, and the TaskRunner used for posting notifications back to this
  // class.
  std::unique_ptr<Core> core_;

  // Caller-supplied message and end-of-file callbacks.
  MessageCallback message_callback_;
  base::Closure eof_callback_;

  // Separate thread used to read from the stream without blocking the main
  // thread. net::FileStream's async API cannot be used here because, on
  // Windows, it requires the file handle to have been opened for overlapped IO.
  base::Thread reader_thread_;
  scoped_refptr<base::SequencedTaskRunner> read_task_runner_;

  // Allows the reader to be deleted safely even when tasks may be pending on
  // it.
  base::WeakPtrFactory<NativeMessagingReader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NativeMessagingReader);
};

}  // namespace remoting

#endif  // REMOTING_HOST_NATIVE_MESSAGING_NATIVE_MESSAGING_READER_H_
