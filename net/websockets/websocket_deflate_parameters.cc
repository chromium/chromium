// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_deflate_parameters.h"

#include <vector>  // for iterating over extension.parameters()

#include "base/strings/string_number_conversions.h"

namespace net {

namespace {

const WebSocketDeflater::ContextTakeOverMode kTakeOverContext =
    WebSocketDeflater::TAKE_OVER_CONTEXT;
const WebSocketDeflater::ContextTakeOverMode kDoNotTakeOverContext =
    WebSocketDeflater::DO_NOT_TAKE_OVER_CONTEXT;

constexpr char kServerNoContextTakeOver[] = "server_no_context_takeover";
constexpr char kClientNoContextTakeOver[] = "client_no_context_takeover";
constexpr char kServerMaxWindowBits[] = "server_max_window_bits";
constexpr char kClientMaxWindowBits[] = "client_max_window_bits";
constexpr char kExtensionName[] = "permessage-deflate";

bool GetWindowBits(const std::string& value, int* window_bits) {
  return !value.empty() && value[0] != '0' &&
         value.find_first_not_of("0123456789") == std::string::npos &&
         base::StringToInt(value, window_bits);
}

bool DuplicateError(const std::string& name, std::string* failure_message) {
  *failure_message =
      "Received duplicate permessage-deflate extension parameter " + name;
  return false;
}

bool InvalidError(const std::string& name, std::string* failure_message) {
  *failure_message = "Received invalid " + name + " parameter";
  return false;
}

}  // namespace

WebSocketExtension WebSocketDeflateParameters::AsExtension() const {
  WebSocketExtension e(kExtensionName);

  if (server_context_take_over_mode_ == kDoNotTakeOverContext)
    e.Add(WebSocketExtension::Parameter(kServerNoContextTakeOver));
  if (client_context_take_over_mode_ == kDoNotTakeOverContext)
    e.Add(WebSocketExtension::Parameter(kClientNoContextTakeOver));
  if (is_server_max_window_bits_specified()) {
    DCHECK(server_max_window_bits_.has_value);
    e.Add(WebSocketExtension::Parameter(
        kServerMaxWindowBits, base::NumberToString(server_max_window_bits())));
  }
  if (is_client_max_window_bits_specified()) {
    if (has_client_max_window_bits_value()) {
      e.Add(WebSocketExtension::Parameter(
          kClientMaxWindowBits,
          base::NumberToString(client_max_window_bits())));
    } else {
      e.Add(WebSocketExtension::Parameter(kClientMaxWindowBits));
    }
  }

  return e;
}

bool WebSocketDeflateParameters::IsValidAsRequest(std::string*) const {
  if (server_max_window_bits_.is_specified) {
    DCHECK(server_max_window_bits_.has_value);
    DCHECK(IsValidWindowBits(server_max_window_bits_.bits));
  }
  if (client_max_window_bits_.is_specified &&
      client_max_window_bits_.has_value) {
    DCHECK(IsValidWindowBits(client_max_window_bits_.bits));
  }
  return true;
}

bool WebSocketDeflateParameters::IsValidAsResponse(
    std::string* failure_message) const {
  if (server_max_window_bits_.is_specified) {
    DCHECK(server_max_window_bits_.has_value);
    DCHECK(IsValidWindowBits(server_max_window_bits_.bits));
  }
  if (client_max_window_bits_.is_specified) {
    if (!client_max_window_bits_.has_value) {
      *failure_message = "client_max_window_bits must have value";
      return false;
    }
    DCHECK(IsValidWindowBits(client_max_window_bits_.bits));
  }

  return true;
}

bool WebSocketDeflateParameters::Initialize(const WebSocketExtension& extension,
                                            std::string* failure_message) {
  *this = WebSocketDeflateParameters();

  if (extension.name() != kExtensionName) {
    *failure_message = "extension name doesn't match";
    return false;
  }
  for (const auto& p : extension.parameters()) {
    if (p.name() == kServerNoContextTakeOver) {
      if (server_context_take_over_mode() == kDoNotTakeOverContext)
        return DuplicateError(p.name(), failure_message);
      if (p.HasValue())
        return InvalidError(p.name(), failure_message);
      SetServerNoContextTakeOver();
    } else if (p.name() == kClientNoContextTakeOver) {
      if (client_context_take_over_mode() == kDoNotTakeOverContext)
        return DuplicateError(p.name(), failure_message);
      if (p.HasValue())
        return InvalidError(p.name(), failure_message);
      SetClientNoContextTakeOver();
    } else if (p.name() == kServerMaxWindowBits) {
      if (server_max_window_bits_.is_specified)
        return DuplicateError(p.name(), failure_message);
      int bits;
      if (!GetWindowBits(p.value(), &bits) || !IsValidWindowBits(bits))
        return InvalidError(p.name(), failure_message);
      SetServerMaxWindowBits(bits);
    } else if (p.name() == kClientMaxWindowBits) {
      if (client_max_window_bits_.is_specified)
        return DuplicateError(p.name(), failure_message);
      if (p.value().empty()) {
        SetClientMaxWindowBits();
      } else {
        int bits;
        if (!GetWindowBits(p.value(), &bits) || !IsValidWindowBits(bits))
          return InvalidError(p.name(), failure_message);
        SetClientMaxWindowBits(bits);
      }
    } else {
      *failure_message =
          "Received an unexpected permessage-deflate extension parameter";
      return false;
    }
  }
  return true;
}

bool WebSocketDeflateParameters::IsCompatibleWith(
    const WebSocketDeflateParameters& response) const {
  const auto& request = *this;
  DCHECK(request.IsValidAsRequest());
  DCHECK(response.IsValidAsResponse());

  // server_no_context_take_over
  if (request.server_context_take_over_mode() == kDoNotTakeOverContext &&
      response.server_context_take_over_mode() == kTakeOverContext) {
    return false;
  }

  // No compatibility check is needed for client_no_context_take_over

  // server_max_window_bits
  if (request.server_max_window_bits_.is_specified) {
    DCHECK(request.server_max_window_bits_.has_value);
    if (!response.server_max_window_bits_.is_specified)
      return false;
    DCHECK(response.server_max_window_bits_.has_value);
    if (request.server_max_window_bits_.bits <
        response.server_max_window_bits_.bits) {
      return false;
    }
  }

  // client_max_window_bits
  if (!request.client_max_window_bits_.is_specified &&
      response.client_max_window_bits_.is_specified) {
    return false;
  }

  return true;
}

}  // namespace net
