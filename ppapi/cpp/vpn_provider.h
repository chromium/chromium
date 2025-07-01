// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VPN_PROVIDER_H_
#define PPAPI_CPP_VPN_PROVIDER_H_

#include "ppapi/c/ppb_vpn_provider.h"
#include "ppapi/cpp/completion_callback.h"

namespace pp {

class InstanceHandle;

/// @file
/// This file defines the VpnProvider interface providing a way to implement a
/// VPN client.
/// Important: This API is available only on Chrome OS.

/// The <code>VpnProvider</code> class enhances the
/// <code>chrome.vpnProvider</code> JavaScript API by providing a high
/// performance path for packet handling.
///
/// Permissions: Apps permission <code>vpnProvider</code> is required for
/// <code>VpnProvider.Bind()</code>.
///
/// Typical usage:
/// - Create a <code>VpnProvider</code> instance.
/// - Register the callback for <code>VpnProvider.ReceivePacket()</code>.
/// - In the extension follow the usual workflow for configuring a VPN
///   connection via the <code>chrome.vpnProvider</code> API until the step for
///   notifying the connection state as "connected".
/// - Bind to the previously created connection using
///   <code>VpnProvider.Bind()</code>.
/// - Notify the connection state as "connected" from JavaScript using
///   <code>chrome.vpnProvider.notifyConnectionStateChanged</code>.
/// - When the steps above are completed without errors, a virtual tunnel is
///   created to the network stack of Chrome OS. IP packets can be sent through
///   the tunnel using <code>VpnProvider.SendPacket()</code> and any packets
///   originating on the Chrome OS device will be received using the callback
///   registered for <code>VpnProvider.ReceivePacket()</code>.
/// - When the user disconnects from the VPN configuration or there is an error
///   the extension will be notfied via
///   <code>chrome.vpnProvider.onPlatformMessage</code>.
class VpnProvider : public Resource {
 public:
  /// Constructs a VpnProvider object.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit VpnProvider(const InstanceHandle& instance);

  /// Destructs a VpnProvider object.
  virtual ~VpnProvider();

  /// Static function for determining whether the browser supports the
  /// <code>VpnProvider</code> interface.
  ///
  /// @return true if the interface is available, false otherwise.
  static bool IsAvailable();

  /// Binds to an existing configuration created from JavaScript by
  /// <code>chrome.vpnProvider.createConfig</code>. All packets will be routed
  /// via <code>SendPacket</code> and <code>ReceivePacket</code>. The user
  /// should register the callback for <code>ReceivePacket</code> before calling
  /// <code>Bind()</code>.
  ///
  /// @param[in] configuration_id The configuration id from the callback of
  /// <code>chrome.vpnProvider.createConfig</code>. This <code>Var</code> must
  /// be of string type.
  ///
  /// @param[in] configuration_name The configuration name as defined by the
  /// user when calling <code>chrome.vpnProvider.createConfig</code>. This
  /// <code>Var</code> must be of string type.
  ///
  /// @param[in] callback A <code>CompletionCallback</code> to be called on
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Returns <code>PP_ERROR_INPROGRESS</code> if a previous call to
  /// <code>Bind()</code> has not completed.
  /// Returns <code>PP_ERROR_BADARGUMENT</code> if the <code>Var</code> type of
  /// either <code>configuration_id</code> or <code>configuration_name</code> is
  /// not of string type.
  /// Returns <code>PP_ERROR_NOACCESS</code> if the caller does the have the
  /// required "vpnProvider" permission.
  /// Returns <code>PP_ERROR_FAILED</code> if <code>connection_id</code> and
  /// <code>connection_name</code> could not be matched with the existing
  /// connection, or if the plugin originates from a different extension than
  /// the one that created the connection.
  int32_t Bind(const Var& configuration_id,
               const Var& configuration_name,
               const CompletionCallback& callback);

  /// Sends an IP packet through the tunnel created for the VPN session. This
  /// will succeed only when the VPN session is owned by the module and
  /// connection is bound.
  ///
  /// @param[in] packet IP packet to be sent to the platform. The
  /// <code>Var</code> must be of ArrayBuffer type.
  ///
  /// @param[in] callback A <code>CompletionCallback</code> to be called on
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Returns <code>PP_ERROR_FAILED</code> if the connection is not bound.
  /// Returns <code>PP_ERROR_INPROGRESS</code> if a previous call to
  /// <code>SendPacket()</code> has not completed.
  /// Returns <code>PP_ERROR_BADARGUMENT</code> if the <code>Var</code> type of
  /// <code>packet</code> is not of ArrayBuffer type.
  int32_t SendPacket(const Var& packet, const CompletionCallback& callback);

  /// Receives an IP packet from the tunnel for the VPN session. This function
  /// only returns a single packet. That is, this function must be called at
  /// least N times to receive N packets, no matter the size of each packet. The
  /// callback should be registered before calling <code>Bind()</code>.
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code> to be
  /// called upon completion of ReceivePacket. It will be passed an ArrayBuffer
  /// type <code>Var</code> containing an IP packet to be sent to the platform.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Returns <code>PP_ERROR_INPROGRESS</code> if a previous call to
  /// <code>ReceivePacket()</code> has not completed.
  int32_t ReceivePacket(const CompletionCallbackWithOutput<Var>& callback);

 private:
  InstanceHandle associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_VPN_PROVIDER_H_
