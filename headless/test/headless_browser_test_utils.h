// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_BROWSER_TEST_UTILS_H_
#define HEADLESS_TEST_HEADLESS_BROWSER_TEST_UTILS_H_

#include <string>

#include "base/strings/string_piece.h"
#include "base/values.h"

namespace simple_devtools_protocol_client {
class SimpleDevToolsProtocolClient;
}

namespace headless {

// Send DevTools command and wait for response by running local
// message loop. This is typically used as a quick and dirty way
// to enable a domain.
base::Value::Dict SendCommandSync(
    simple_devtools_protocol_client::SimpleDevToolsProtocolClient&
        devtools_client,
    const std::string& command);
base::Value::Dict SendCommandSync(
    simple_devtools_protocol_client::SimpleDevToolsProtocolClient&
        devtools_client,
    const std::string& command,
    base::Value::Dict params);

// TODO(kvitekp): Consider moving these to Simple CDP client header.

// Convenience function to create a single key/value Dict.
template <typename T>
base::Value::Dict Param(base::StringPiece key, T&& value) {
  base::Value::Dict param;
  param.Set(key, std::move(value));
  return param;
}

// Convenience functions to retrieve values from a |result| Dict.
bool ResultError(const base::Value::Dict& result,
                 int* code = nullptr,
                 std::string* message = nullptr);
std::string ResultString(const base::Value::Dict& result,
                         base::StringPiece key);
int ResultInt(const base::Value::Dict& result, base::StringPiece key);
bool ResultBool(const base::Value::Dict& result, base::StringPiece key);

bool ResultHas(const base::Value::Dict& result, base::StringPiece key);

// Convenience functions to retrieve values from a |params| Dict.
std::string ParamsString(const base::Value::Dict& params,
                         base::StringPiece key);
int ParamsInt(const base::Value::Dict& params, base::StringPiece key);
bool ParamsBool(const base::Value::Dict& params, base::StringPiece key);

bool ParamHas(const base::Value::Dict& params, base::StringPiece key);

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_BROWSER_TEST_UTILS_H_
