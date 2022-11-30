// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_UDP_SOCKET_H_
#define PPAPI_CPP_UDP_SOCKET_H_

#include <stdint.h>

#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class InstanceHandle;
class Var;

template <typename T> class CompletionCallbackWithOutput;

/// The <code>UDPSocket</code> class provides UDP socket operations.
///
/// Permissions: Apps permission <code>socket</code> with subrule
/// <code>udp-bind</code> is required for <code>Bind()</code>; subrule
/// <code>udp-send-to</code> is required for <code>SendTo()</code>.
/// For more details about network communication permissions, please see:
/// http://developer.chrome.com/apps/app_network.html
class UDPSocket : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>UDPSocket</code>
  /// object.
  UDPSocket();

  /// A constructor used to create a <code>UDPSocket</code> object.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit UDPSocket(const InstanceHandle& instance);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_UDPSocket</code> resource.
  UDPSocket(PassRef, PP_Resource resource);

  /// The copy constructor for <code>UDPSocket</code>.
  ///
  /// @param[in] other A reference to another <code>UDPSocket</code>.
  UDPSocket(const UDPSocket& other);

  /// The destructor.
  virtual ~UDPSocket();

  /// The assignment operator for <code>UDPSocket</code>.
  ///
  /// @param[in] other A reference to another <code>UDPSocket</code>.
  ///
  /// @return A reference to this <code>UDPSocket</code> object.
  UDPSocket& operator=(const UDPSocket& other);

  /// Static function for determining whether the browser supports the
  /// <code>PPB_UDPSocket</code> interface.
  ///
  /// @return true if the interface is available, false otherwise.
  static bool IsAvailable();

  /// Binds the socket to the given address.
  ///
  /// @param[in] addr A <code>NetAddress</code> object.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// <code>PP_ERROR_NOACCESS</code> will be returned if the caller doesn't have
  /// required permissions. <code>PP_ERROR_ADDRESS_IN_USE</code> will be
  /// returned if the address is already in use.
  int32_t Bind(const NetAddress& addr,
               const CompletionCallback& callback);

  /// Get the address that the socket is bound to. The socket must be bound.
  ///
  /// @return A <code>NetAddress</code> object. The object will be null
  /// (i.e., is_null() returns true) on failure.
  NetAddress GetBoundAddress();

  /// Receives data from the socket and stores the source address. The socket
  /// must be bound.
  ///
  /// <strong>Caveat:</strong> You should be careful about the lifetime of
  /// <code>buffer</code>. Typically you will use a
  /// <code>CompletionCallbackFactory</code> to scope callbacks to the lifetime
  /// of your class. When your class goes out of scope, the callback factory
  /// will not actually cancel the operation, but will rather just skip issuing
  /// the callback on your class. This means that if the underlying
  /// <code>PPB_UDPSocket</code> resource outlives your class, the browser
  /// will still try to write into your buffer when the operation completes.
  /// The buffer must be kept valid until then to avoid memory corruption.
  /// If you want to release the buffer while the <code>RecvFrom()</code> call
  /// is still pending, you should call <code>Close()</code> to ensure that the
  /// buffer won't be accessed in the future.
  ///
  /// @param[out] buffer The buffer to store the received data on success. It
  /// must be at least as large as <code>num_bytes</code>.
  /// @param[in] num_bytes The number of bytes to receive.
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code> to be
  /// called upon completion.
  ///
  /// @return A non-negative number on success to indicate how many bytes have
  /// been received; otherwise, an error code from <code>pp_errors.h</code>.
  int32_t RecvFrom(
      char* buffer,
      int32_t num_bytes,
      const CompletionCallbackWithOutput<NetAddress>& callback);

  /// Sends data to a specific destination. The socket must be bound.
  ///
  /// @param[in] buffer The buffer containing the data to send.
  /// @param[in] num_bytes The number of bytes to send.
  /// @param[in] addr A <code>NetAddress</code> object holding the destination
  /// address.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return A non-negative number on success to indicate how many bytes have
  /// been sent; otherwise, an error code from <code>pp_errors.h</code>.
  /// <code>PP_ERROR_NOACCESS</code> will be returned if the caller doesn't have
  /// required permissions.
  /// <code>PP_ERROR_INPROGRESS</code> will be returned if the socket is busy
  /// sending. The caller should wait until a pending send completes before
  /// retrying.
  int32_t SendTo(const char* buffer,
                 int32_t num_bytes,
                 const NetAddress& addr,
                 const CompletionCallback& callback);

  /// Cancels all pending reads and writes, and closes the socket. Any pending
  /// callbacks will still run, reporting <code>PP_ERROR_ABORTED</code> if
  /// pending IO was interrupted. After a call to this method, no output
  /// paramters passed into previous <code>RecvFrom()</code> calls will be
  /// accessed. It is not valid to call <code>Bind()</code> again.
  ///
  /// The socket is implicitly closed if it is destroyed, so you are not
  /// required to call this method.
  void Close();

  /// Sets a socket option on the UDP socket.
  /// Please see the <code>PP_UDPSocket_Option</code> description for option
  /// names, value types and allowed values.
  ///
  /// @param[in] name The option to set.
  /// @param[in] value The option value to set.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t SetOption(PP_UDPSocket_Option name,
                    const Var& value,
                    const CompletionCallback& callback);

  /// Joins the multicast group with address specified by <code>group</code>
  /// parameter, which is expected to be a <code>NetAddress</code> object.
  ///
  /// @param[in] group A <code>NetAddress</code> corresponding to the network
  /// address of the multicast group.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t JoinGroup(const NetAddress& group,
                    const CompletionCallback callback);

  /// Leaves the multicast group with address specified by <code>group</code>
  /// parameter, which is expected to be a <code>NetAddress</code> object.
  ///
  /// @param[in] group A <code>NetAddress</code> corresponding to the network
  /// address of the multicast group.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t LeaveGroup(const NetAddress& group,
                     const CompletionCallback callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_UDP_SOCKET_H_
