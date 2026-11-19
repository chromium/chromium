// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_EXTENSION_H_
#define NET_WEBSOCKETS_WEBSOCKET_EXTENSION_H_

#include <string>
#include <utility>
#include <vector>

#include "net/base/net_export.h"

namespace net {

// A WebSocketExtension instance represents a WebSocket extension specified
// in RFC6455.
class NET_EXPORT_PRIVATE WebSocketExtension {
 public:
  // Note that RFC6455 does not allow a parameter with an empty value.
  class NET_EXPORT_PRIVATE Parameter final {
   public:
    // Construct a parameter which does not have a value.
    explicit Parameter(std::string name);
    // Construct a parameter with a non-empty value.
    Parameter(std::string name, std::string value);

    Parameter(const Parameter&);
    Parameter& operator=(const Parameter&);
    Parameter(Parameter&&) noexcept;
    Parameter& operator=(Parameter&&) noexcept;
    ~Parameter();

    friend bool operator==(const Parameter&, const Parameter&) = default;

    bool HasValue() const { return !value_.empty(); }
    const std::string& name() const { return name_; }
    const std::string& value() const { return value_; }

   private:
    std::string name_;
    std::string value_;
  };

  explicit WebSocketExtension(std::string name);

  WebSocketExtension(const WebSocketExtension&);
  WebSocketExtension& operator=(const WebSocketExtension&);
  WebSocketExtension(WebSocketExtension&&) noexcept;
  WebSocketExtension& operator=(WebSocketExtension&&) noexcept;
  ~WebSocketExtension();

  void Add(Parameter parameter) { parameters_.push_back(std::move(parameter)); }
  const std::string& name() const { return name_; }
  const std::vector<Parameter>& parameters() const { return parameters_; }

  // Equivalent() returns true if `name_` and `parameters_` are the same, even
  // if the order of keys in `parameters_` is different.
  bool Equivalent(const WebSocketExtension& other) const;

  std::string ToString() const;

 private:
  std::string name_;
  std::vector<Parameter> parameters_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_EXTENSION_H_
