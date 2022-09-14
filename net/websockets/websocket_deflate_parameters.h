// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PARAMETERS_H_
#define NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PARAMETERS_H_

#include <stdint.h>

#include <string>

#include "base/check.h"
#include "net/base/net_export.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_extension.h"

namespace net {

// A WebSocketDeflateParameters represents permessage-deflate extension
// parameters. This class is used either for request and response.
class NET_EXPORT_PRIVATE WebSocketDeflateParameters {
 public:
  using ContextTakeOverMode = WebSocketDeflater::ContextTakeOverMode;

  // Returns a WebSocketExtension instance containing the parameters stored in
  // this object.
  WebSocketExtension AsExtension() const;

  // Returns true when succeeded.
  // Returns false and stores the failure message to |failure_message|
  // otherwise.
  // Note that even if this function succeeds it is not guaranteed that the
  // object is valid. To check it, call IsValidAsRequest or IsValidAsResponse.
  bool Initialize(const WebSocketExtension& input,
                  std::string* failure_message);

  // Returns true when |*this| and |response| are compatible.
  // |*this| must be valid as a request and |response| must be valid as a
  // response.
  bool IsCompatibleWith(const WebSocketDeflateParameters& response) const;

  bool IsValidAsRequest(std::string* failure_message) const;
  bool IsValidAsResponse(std::string* failure_message) const;
  bool IsValidAsRequest() const {
    std::string message;
    return IsValidAsRequest(&message);
  }
  bool IsValidAsResponse() const {
    std::string message;
    return IsValidAsResponse(&message);
  }

  ContextTakeOverMode server_context_take_over_mode() const {
    return server_context_take_over_mode_;
  }
  ContextTakeOverMode client_context_take_over_mode() const {
    return client_context_take_over_mode_;
  }
  bool is_server_max_window_bits_specified() const {
    return server_max_window_bits_.is_specified;
  }
  int server_max_window_bits() const {
    DCHECK(is_server_max_window_bits_specified());
    return server_max_window_bits_.bits;
  }
  bool is_client_max_window_bits_specified() const {
    return client_max_window_bits_.is_specified;
  }
  bool has_client_max_window_bits_value() const {
    DCHECK(is_client_max_window_bits_specified());
    return client_max_window_bits_.has_value;
  }
  int client_max_window_bits() const {
    DCHECK(has_client_max_window_bits_value());
    return client_max_window_bits_.bits;
  }
  void SetServerNoContextTakeOver() {
    server_context_take_over_mode_ =
        WebSocketDeflater::DO_NOT_TAKE_OVER_CONTEXT;
  }
  void SetClientNoContextTakeOver() {
    client_context_take_over_mode_ =
        WebSocketDeflater::DO_NOT_TAKE_OVER_CONTEXT;
  }
  // |bits| must be valid as a max_window_bits value.
  void SetServerMaxWindowBits(int bits) {
    DCHECK(IsValidWindowBits(bits));
    server_max_window_bits_ = WindowBits(bits, true, true);
  }
  void SetClientMaxWindowBits() {
    client_max_window_bits_ = WindowBits(0, true, false);
  }
  // |bits| must be valid as a max_window_bits value.
  void SetClientMaxWindowBits(int bits) {
    DCHECK(IsValidWindowBits(bits));
    client_max_window_bits_ = WindowBits(bits, true, true);
  }

  int PermissiveServerMaxWindowBits() const {
    return server_max_window_bits_.PermissiveBits();
  }
  int PermissiveClientMaxWindowBits() const {
    return client_max_window_bits_.PermissiveBits();
  }

  // Return true if |bits| is valid as a max_window_bits value.
  static bool IsValidWindowBits(int bits) { return 8 <= bits && bits <= 15; }

 private:
  struct WindowBits {
    WindowBits() : WindowBits(0, false, false) {}
    WindowBits(int16_t bits, bool is_specified, bool has_value)
        : bits(bits), is_specified(is_specified), has_value(has_value) {}

    int PermissiveBits() const {
      return (is_specified && has_value) ? bits : 15;
    }

    int16_t bits;
    // True when "window bits" parameter appears in the parameters.
    bool is_specified;
    // True when "window bits" parameter has the value.
    bool has_value;
  };

  // |server_context_take_over_mode| is set to DO_NOT_TAKE_OVER_CONTEXT if and
  // only if |server_no_context_takeover| is set in the parameters.
  ContextTakeOverMode server_context_take_over_mode_ =
      WebSocketDeflater::TAKE_OVER_CONTEXT;
  // |client_context_take_over_mode| is set to DO_NOT_TAKE_OVER_CONTEXT if and
  // only if |client_no_context_takeover| is set in the parameters.
  ContextTakeOverMode client_context_take_over_mode_ =
      WebSocketDeflater::TAKE_OVER_CONTEXT;
  WindowBits server_max_window_bits_;
  WindowBits client_max_window_bits_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_DEFLATE_PARAMETERS_H_
