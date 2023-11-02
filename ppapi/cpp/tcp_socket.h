// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_TCP_SOCKET_H_
#define PPAPI_CPP_TCP_SOCKET_H_

#include <stdint.h>

#include "ppapi/c/ppb_tcp_socket.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class InstanceHandle;

template <typename T> class CompletionCallbackWithOutput;

/// The <code>TCPSocket</code> class provides TCP socket operations.
///
/// Permissions: Apps permission <code>socket</code> with subrule
/// <code>tcp-connect</code> is required for <code>Connect()</code>; subrule
/// <code>tcp-listen</code> is required for <code>Listen()</code>.
/// For more details about network communication permissions, please see:
/// http://developer.chrome.com/apps/app_network.html
class TCPSocket : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>TCPSocket</code>
  /// object.
  TCPSocket();

  /// A constructor used to create a <code>TCPSocket</code> object.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit TCPSocket(const InstanceHandle& instance);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_TCPSocket</code> resource.
  TCPSocket(PassRef, PP_Resource resource);

  /// The copy constructor for <code>TCPSocket</code>.
  ///
  /// @param[in] other A reference to another <code>TCPSocket</code>.
  TCPSocket(const TCPSocket& other);

  /// The destructor.
  virtual ~TCPSocket();

  /// The assignment operator for <code>TCPSocket</code>.
  ///
  /// @param[in] other A reference to another <code>TCPSocket</code>.
  ///
  /// @return A reference to this <code>TCPSocket</code> object.
  TCPSocket& operator=(const TCPSocket& other);

  /// Static function for determining whether the browser supports the
  /// <code>PPB_TCPSocket</code> interface.
  ///
  /// @return true if the interface is available, false otherwise.
  static bool IsAvailable();

  /// Binds the socket to the given address. The socket must not be bound.
  ///
  /// @param[in] addr A <code>NetAddress</code> object.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>,
  /// including (but not limited to):
  /// - <code>PP_ERROR_ADDRESS_IN_USE</code>: the address is already in use.
  /// - <code>PP_ERROR_ADDRESS_INVALID</code>: the address is invalid.
  int32_t Bind(const NetAddress& addr, const CompletionCallback& callback);

  /// Connects the socket to the given address. The socket must not be
  /// listening. Binding the socket beforehand is optional.
  ///
  /// @param[in] addr A <code>NetAddress</code> object.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>,
  /// including (but not limited to):
  /// - <code>PP_ERROR_NOACCESS</code>: the caller doesn't have required
  ///   permissions.
  /// - <code>PP_ERROR_ADDRESS_UNREACHABLE</code>: <code>addr</code> is
  ///   unreachable.
  /// - <code>PP_ERROR_CONNECTION_REFUSED</code>: the connection attempt was
  ///   refused.
  /// - <code>PP_ERROR_CONNECTION_FAILED</code>: the connection attempt failed.
  /// - <code>PP_ERROR_CONNECTION_TIMEDOUT</code>: the connection attempt timed
  ///   out.
  ///
  /// Since version 1.1, if the socket is listening/connected or has a pending
  /// listen/connect request, <code>Connect()</code> will fail without starting
  /// a connection attempt. Otherwise, any failure during the connection attempt
  /// will cause the socket to be closed.
  int32_t Connect(const NetAddress& addr, const CompletionCallback& callback);

  /// Gets the local address of the socket, if it is bound.
  ///
  /// @return A <code>NetAddress</code> object. The object will be null
  /// (i.e., is_null() returns true) on failure.
  NetAddress GetLocalAddress() const;

  /// Gets the remote address of the socket, if it is connected.
  ///
  /// @return A <code>NetAddress</code> object. The object will be null
  /// (i.e., is_null() returns true) on failure.
  NetAddress GetRemoteAddress() const;

  /// Reads data from the socket. The socket must be connected. It may perform a
  /// partial read.
  ///
  /// <strong>Caveat:</strong> You should be careful about the lifetime of
  /// <code>buffer</code>. Typically you will use a
  /// <code>CompletionCallbackFactory</code> to scope callbacks to the lifetime
  /// of your class. When your class goes out of scope, the callback factory
  /// will not actually cancel the operation, but will rather just skip issuing
  /// the callback on your class. This means that if the underlying
  /// <code>PPB_TCPSocket</code> resource outlives your class, the browser
  /// will still try to write into your buffer when the operation completes.
  /// The buffer must be kept valid until then to avoid memory corruption.
  /// If you want to release the buffer while the <code>Read()</code> call is
  /// still pending, you should call <code>Close()</code> to ensure that the
  /// buffer won't be accessed in the future.
  ///
  /// @param[out] buffer The buffer to store the received data on success. It
  /// must be at least as large as <code>bytes_to_read</code>.
  /// @param[in] bytes_to_read The number of bytes to read.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return A non-negative number on success to indicate how many bytes have
  /// been read, 0 means that end-of-file was reached; otherwise, an error code
  /// from <code>pp_errors.h</code>.
  int32_t Read(char* buffer,
               int32_t bytes_to_read,
               const CompletionCallback& callback);

  /// Writes data to the socket. The socket must be connected. It may perform a
  /// partial write.
  ///
  /// @param[in] buffer The buffer containing the data to write.
  /// @param[in] bytes_to_write The number of bytes to write.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return A non-negative number on success to indicate how many bytes have
  /// been written; otherwise, an error code from <code>pp_errors.h</code>.
  int32_t Write(const char* buffer,
                int32_t bytes_to_write,
                const CompletionCallback& callback);

  /// Starts listening. The socket must be bound and not connected.
  ///
  /// @param[in] backlog A hint to determine the maximum length to which the
  /// queue of pending connections may grow.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>,
  /// including (but not limited to):
  /// - <code>PP_ERROR_NOACCESS</code>: the caller doesn't have required
  ///   permissions.
  /// - <code>PP_ERROR_ADDRESS_IN_USE</code>: Another socket is already
  ///   listening on the same port.
  int32_t Listen(int32_t backlog,
                 const CompletionCallback& callback);

  /// Accepts a connection. The socket must be listening.
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code> to be
  /// called upon completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>,
  /// including (but not limited to):
  /// - <code>PP_ERROR_CONNECTION_ABORTED</code>: A connection has been aborted.
  int32_t Accept(const CompletionCallbackWithOutput<TCPSocket>& callback);

  /// Cancels all pending operations and closes the socket. Any pending
  /// callbacks will still run, reporting <code>PP_ERROR_ABORTED</code> if
  /// pending IO was interrupted. After a call to this method, no output buffer
  /// pointers passed into previous <code>Read()</code> or <code>Accept()</code>
  /// calls will be accessed. It is not valid to call <code>Connect()</code> or
  /// <code>Listen()</code> again.
  ///
  /// The socket is implicitly closed if it is destroyed, so you are not
  /// required to call this method.
  void Close();

  /// Sets a socket option on the TCP socket.
  /// Please see the <code>PP_TCPSocket_Option</code> description for option
  /// names, value types and allowed values.
  ///
  /// @param[in] name The option to set.
  /// @param[in] value The option value to set.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t SetOption(PP_TCPSocket_Option name,
                    const Var& value,
                    const CompletionCallback& callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_TCP_SOCKET_H_
