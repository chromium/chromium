// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_BROWSER_TEST_UTILS_H_
#define HEADLESS_TEST_HEADLESS_BROWSER_TEST_UTILS_H_

#include <string>
#include <string_view>

#include "base/values.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace simple_devtools_protocol_client {
class SimpleDevToolsProtocolClient;
}

namespace headless {

class HeadlessWebContents;

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

// Synchronously evaluates a script and returns the result.
base::Value::Dict EvaluateScript(HeadlessWebContents* web_contents,
                                 const std::string& script);

// Synchronously waits for a tab to finish loading and optionally retrieves
// an error.
bool WaitForLoad(HeadlessWebContents* web_contents,
                 net::Error* error = nullptr);

// Synchronously waits for a tab to finish loading and to gain focus.
void WaitForLoadAndGainFocus(HeadlessWebContents* web_contents);

// Convenience function to create a single key/value Dict.
template <typename T>
base::Value::Dict Param(std::string_view key, T&& value) {
  base::Value::Dict param;
  param.Set(key, std::move(value));
  return param;
}

// Convenience functions to retrieve values from a base::Value::Dict and
// CHECK fail if the specified path is not found.
std::string DictString(const base::Value::Dict& dict, std::string_view path);
int DictInt(const base::Value::Dict& dict, std::string_view path);
bool DictBool(const base::Value::Dict& dict, std::string_view path);
bool DictHas(const base::Value::Dict& dict, std::string_view path);

// A custom GMock matcher which matches if a base::Value::Dict has
// a path |path| that is equal to |value|.
testing::Matcher<const base::Value::Dict&> DictHasPathValue(
    const std::string& path,
    base::Value expected_value);

template <typename T>
testing::Matcher<const base::Value::Dict&> DictHasValue(const std::string& path,
                                                        T expected_value) {
  return DictHasPathValue(path, base::Value(expected_value));
}

// A custom GMock matcher which matches if a base::Value::Dict has
// the key |key|.
testing::Matcher<const base::Value::Dict&> DictHasKey(const std::string& key);

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_BROWSER_TEST_UTILS_H_
