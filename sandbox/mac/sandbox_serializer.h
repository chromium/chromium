// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SANDBOX_SERIALIZER_H_
#define SANDBOX_MAC_SANDBOX_SERIALIZER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt_export.h"

namespace sandbox {

// This is a helper to build, serialize, and deserialize sandbox policies.
class SEATBELT_EXPORT SandboxSerializer {
 public:
  // See `CanCacheSandboxPolicy()` for more information on when each target type
  // is used.
  enum class Target {
    // The result of serialization is a string containing the policy
    // string source and a map of key/value pairs.
    kSource,

    // The result of serialization is a string containing a sealed,
    // compiled, binary sandbox policy that can be applied immediately.
    kCompiled,
    kMaxValue = kCompiled
  };

  struct DeserializedPolicy {
    DeserializedPolicy();
    DeserializedPolicy(DeserializedPolicy&& other);
    DeserializedPolicy& operator=(DeserializedPolicy&& other) = default;
    ~DeserializedPolicy();
    DeserializedPolicy(const DeserializedPolicy& other) = delete;
    DeserializedPolicy& operator=(const DeserializedPolicy& other) = delete;

    Target mode;
    std::string profile;
    std::vector<std::string> params;
  };

  // Creates a serializer with the specified target mode.
  explicit SandboxSerializer(Target mode);

  ~SandboxSerializer();
  SandboxSerializer(const SandboxSerializer& other) = delete;
  SandboxSerializer& operator=(const SandboxSerializer& other) = delete;

  // Sets the policy source string.
  void SetProfile(const std::string& profile);

  // Inserts a boolean into the parameters key/value map. A duplicate key is not
  // allowed, and will cause the function to return false. The value is not
  // inserted in this case.
  [[nodiscard]] bool SetBooleanParameter(const std::string& key, bool value);

  // Inserts a string into the parameters key/value map. A duplicate key is not
  // allowed, and will cause the function to return false. The value is not
  // inserted in this case.
  [[nodiscard]] bool SetParameter(const std::string& key,
                                  const std::string& value);

  // Compiles the policy into a string suitable for wire transfer. Returns true
  // on success, with `policy` set, or returns false on error with a message in
  // the `error` parameter.
  [[nodiscard]] bool SerializePolicy(std::string& serialized_policy,
                                     std::string& error);

  // Attempts to deserialize `serialized_policy` and returns the policy if
  // deserialization is successful, or `std::nullopt` if it fails, with a
  // description of the failure in `error`.
  [[nodiscard]] static std::optional<SandboxSerializer::DeserializedPolicy>
  DeserializePolicy(const std::string& serialized_policy, std::string& error);

  // Applies the given sandbox policy, and returns whether or not the operation
  // succeeds.
  [[nodiscard]] static bool ApplySerializedPolicy(
      const std::string& serialized_policy);

 private:
  const Target mode_;

  std::string profile_;
  std::map<std::string, std::string> source_params_;

  Seatbelt::Parameters params_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_SANDBOX_SERIALIZER_H_
