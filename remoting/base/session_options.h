// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SESSION_OPTIONS_H_
#define REMOTING_BASE_SESSION_OPTIONS_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"

namespace remoting {

// Session based host options sending from client. This class parses and stores
// session configuration from client side to control the behavior of other host
// components.
class SessionOptions final {
 public:
  SessionOptions();
  SessionOptions(const SessionOptions& other);
  SessionOptions(SessionOptions&& other);
  explicit SessionOptions(const std::string& parameter);

  ~SessionOptions();

  SessionOptions& operator=(const SessionOptions& other);
  SessionOptions& operator=(SessionOptions&& other);

  // Appends one key-value pair into current instance.
  void Append(const std::string& key, const std::string& value);

  // Retrieves the value of |key|. Returns a true Optional if |key| has been
  // found, value of the Optional will be set to corresponding value.
  std::optional<std::string> Get(const std::string& key) const;

  // Retrieves the value of |key|. Returns a true Optional if |key| has been
  // found and the corresponding value can be converted to a boolean value.
  // "true", "1" or empty will be converted to true, "false" or "0" will be
  // converted to false.
  std::optional<bool> GetBool(const std::string& key) const;

  // Equivalent to GetBool(key).value_or(false).
  bool GetBoolValue(const std::string& key) const;

  // Retrieves the value of |key|. Returns a true Optional if |key| has been
  // found and the corresponding value can be converted to an integer.
  std::optional<int> GetInt(const std::string& key) const;

  // Returns a string to represent current instance. Consumers can rebuild an
  // exactly same instance with Import() function.
  std::string Export() const;

  // Overwrite current instance with |parameter|, which is a string returned by
  // Export() function. So a parent process can send SessionOptions to a
  // child process.
  void Import(const std::string& parameter);

 private:
  base::flat_map<std::string, std::string> options_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_SESSION_OPTIONS_H_
