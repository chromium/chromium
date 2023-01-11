// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_READER_IMPL_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_READER_IMPL_H_

#include <memory>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "remoting/host/security_key/security_key_message.h"
#include "remoting/host/security_key/security_key_message_reader.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// SecurityKeyMessageReader implementation that receives messages from
// a pipe.
class SecurityKeyMessageReaderImpl : public SecurityKeyMessageReader {
 public:
  explicit SecurityKeyMessageReaderImpl(base::File input_file);

  SecurityKeyMessageReaderImpl(const SecurityKeyMessageReaderImpl&) = delete;
  SecurityKeyMessageReaderImpl& operator=(const SecurityKeyMessageReaderImpl&) =
      delete;

  ~SecurityKeyMessageReaderImpl() override;

  // SecurityKeyMessageReader interface.
  void Start(const SecurityKeyMessageCallback& message_callback,
             base::OnceClosure error_callback) override;

 private:
  // Reads a message from the remote_security_key process and passes it to
  // |message_callback_| on the originating thread. Run on |read_task_runner_|.
  void ReadMessage();

  // Reads the nubmer of bytes indicated by |bytes_to_read| into |buffer| from
  // |read_stream_|.  Returns true if all bytes were retrieved successfully.
  bool ReadFromStream(char* buffer, size_t bytes_to_read);

  // Callback run on |read_task_runner_| when an error occurs or EOF is reached.
  void NotifyError();

  // Used for callbacks on the appropriate task runner to signal status changes.
  // These callbacks are invoked on |main_task_runner_|.
  void InvokeMessageCallback(std::unique_ptr<SecurityKeyMessage> message);
  void InvokeErrorCallback();

  base::File read_stream_;

  // Caller-supplied message and error callbacks.
  SecurityKeyMessageCallback message_callback_;
  base::OnceClosure error_callback_;

  // Thread used for blocking IO operations.
  base::Thread reader_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> read_task_runner_;

  base::WeakPtr<SecurityKeyMessageReaderImpl> reader_;
  base::WeakPtrFactory<SecurityKeyMessageReaderImpl> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_READER_IMPL_H_
