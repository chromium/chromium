// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_SOCKET_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_SOCKET_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"

namespace base {
class OneShotTimer;
class TimeDelta;
}  // namespace base

namespace net {
class DrainableIOBuffer;
class IOBufferWithSize;
class StreamSocket;
}  // namespace net

namespace remoting {

// Class that manages reading requests and sending responses. The socket can
// only handle receiving one request at a time. It expects to receive no extra
// bytes over the wire, which is checked by IsRequestTooLarge method.
class SecurityKeySocket {
 public:
  SecurityKeySocket(std::unique_ptr<net::StreamSocket> socket,
                    base::TimeDelta timeout,
                    base::OnceClosure timeout_callback);

  SecurityKeySocket(const SecurityKeySocket&) = delete;
  SecurityKeySocket& operator=(const SecurityKeySocket&) = delete;

  ~SecurityKeySocket();

  // Returns false if the request has not yet completed, or is too large to be
  // processed. Otherwise, the cached request data is copied into |data_out| and
  // the internal buffer resets and is ready for the next request.
  bool GetAndClearRequestData(std::string* data_out);

  // Sends response data to the socket.
  void SendResponse(const std::string& data);

  // Sends an SSH error code to the socket.
  void SendSshError();

  // |request_received_callback| is used to notify the caller that request data
  // has been fully read, and caller is to use GetAndClearRequestData method to
  // get the request data.
  void StartReadingRequest(base::OnceClosure request_received_callback);

  bool socket_read_error() const { return socket_read_error_; }

 private:
  // Called when bytes are written to |socket_|.
  void OnDataWritten(int result);

  // Continues writing to |socket_| if needed.
  void DoWrite();

  // Called when bytes are read from |socket_|.
  void OnDataRead(int bytes_read);

  // Continues to read.
  void DoRead();

  // Returns true if the current request is complete.
  bool IsRequestComplete() const;

  // Returns true if the stated request size is larger than the allowed maximum.
  bool IsRequestTooLarge() const;

  // Returns the stated request length.
  size_t GetRequestLength() const;

  // Returns the response length bytes.
  std::string GetResponseLengthAsBytes(const std::string& response) const;

  // Resets the socket activity timer.
  void ResetTimer();

  // Ensures SecurityKeySocketSocket methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // The socket.
  std::unique_ptr<net::StreamSocket> socket_;

  // Invoked when request data has been read.
  base::OnceClosure request_received_callback_;

  // Indicates whether the socket is being used to wait for a request.
  bool waiting_for_request_ = false;

  // Indicates whether an error was encountered while reading from the socket.
  bool socket_read_error_ = false;

  // Request data.
  std::vector<char> request_data_;

  scoped_refptr<net::DrainableIOBuffer> write_buffer_;

  scoped_refptr<net::IOBufferWithSize> read_buffer_;

  // The activity timer.
  std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_SOCKET_H_
