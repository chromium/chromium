// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/sandbox_serializer.h"

#include <string>
#include <vector>

#include "sandbox/mac/sandbox_logging.h"
#include "sandbox/mac/seatbelt.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

namespace sandbox {

namespace {

// TODO(crbug.com/393161124): replace with different encoding.
void EncodeVarInt(uint64_t from, std::string* into) {
  do {
    unsigned char c = from & 0x7f;
    from >>= 7;
    if (from) {
      c |= 0x80;
    }
    into->push_back(c);
  } while (from);
}

bool DecodeVarInt(std::string_view* from, uint64_t* into) {
  std::string_view::const_iterator it = from->begin();
  int shift = 0;
  uint64_t ret = 0;
  do {
    // Shifting 32 or more bits is undefined behavior.
    if (it == from->end() || shift >= 32) {
      return false;
    }

    unsigned char c = *it;
    ret |= static_cast<uint64_t>(c & 0x7f) << shift;
    shift += 7;
  } while (*it++ & 0x80);
  *into = ret;
  from->remove_prefix(it - from->begin());
  return true;
}

void EncodeString(const std::string& value, std::string* into) {
  EncodeVarInt(value.length(), into);
  into->append(value);
}

bool DecodeString(std::string_view* slice, std::string* value) {
  uint64_t length;
  if (!DecodeVarInt(slice, &length) || length < 0) {
    return false;
  }
  size_t size = length;
  if (slice->size() < size) {
    return false;
  }

  value->assign(slice->data(), size);
  slice->remove_prefix(size);
  return true;
}

}  // namespace

SandboxSerializer::SandboxSerializer(Target mode) : mode_(mode) {
  if (mode_ == Target::kCompiled) {
    params_ = Seatbelt::Parameters::Create();
  }
}

SandboxSerializer::~SandboxSerializer() = default;

void SandboxSerializer::SetProfile(const std::string& profile) {
  profile_ = profile;
}

bool SandboxSerializer::SetBooleanParameter(const std::string& key,
                                            bool value) {
  return SetParameter(key, value ? "TRUE" : "FALSE");
}

bool SandboxSerializer::SetParameter(const std::string& key,
                                     const std::string& value) {
  // Regardless of the mode, add the strings to the map because
  // Seatbelt::Parameters::Set does not copy the strings, which means temporary
  // std::string references need to be owned somewhere.
  auto it = source_params_.insert({key, value});

  if (mode_ == Target::kCompiled && it.second) {
    if (!params_.Set(it.first->first.c_str(), it.first->second.c_str())) {
      source_params_.erase(it.first);
      return false;
    }
  }

  return it.second;
}

bool SandboxSerializer::SerializePolicy(std::string& serialized_policy,
                                        std::string& error) {
  constexpr size_t kBytesForModeVarInt = 1;
  constexpr size_t kMaxBytesForVarInt = (sizeof(uint64_t) * 8 + 6) / 7;
  serialized_policy.clear();

  switch (mode_) {
    case Target::kSource:
      serialized_policy.reserve(kBytesForModeVarInt + kMaxBytesForVarInt +
                                profile_.size() + 512 * source_params_.size());
      EncodeVarInt(static_cast<int>(Target::kSource), &serialized_policy);
      EncodeString(profile_, &serialized_policy);
      for (const auto& [key, value] : source_params_) {
        EncodeString(key, &serialized_policy);
        EncodeString(value, &serialized_policy);
      }
      serialized_policy.shrink_to_fit();
      return true;
    case Target::kCompiled:
      std::string compiled;
      if (!Seatbelt::Compile(profile_.c_str(), params_, compiled, &error)) {
        return false;
      }
      serialized_policy.reserve(kBytesForModeVarInt + kMaxBytesForVarInt +
                                compiled.size());
      EncodeVarInt(static_cast<int>(Target::kCompiled), &serialized_policy);
      EncodeString(compiled, &serialized_policy);
      return true;
  }
}

// static
bool SandboxSerializer::ApplySerializedPolicy(
    const std::string& serialized_policy) {
  std::string_view policy = serialized_policy;
  uint64_t mode;
  if (!DecodeVarInt(&policy, &mode)) {
    logging::Error(
        "SandboxSerializer: unexpected serialized policy mode bytes");
    return false;
  }
  if (mode == static_cast<int>(Target::kCompiled)) {
    std::string compiled;
    if (!DecodeString(&policy, &compiled)) {
      logging::Error(
          "SandboxSerializer: could not decode compiled policy string");
      return false;
    }
    std::string error;
    if (!Seatbelt::ApplyCompiledProfile(compiled, &error)) {
      logging::Error("SandboxSerializer: Failed to apply compiled policy: %s",
                     error.c_str());
      return false;
    }
    return true;
  }

  if (mode != static_cast<int>(Target::kSource)) {
    logging::Error("SandboxSerializer: got unexpected policy mode source");
    return false;
  }

  std::string profile;
  if (!DecodeString(&policy, &profile)) {
    logging::Error(
        "SandboxSerializer: could not decode source mode profile string");
    return false;
  }
  std::vector<std::string> params;
  while (!policy.empty()) {
    std::string key, value;
    if (!DecodeString(&policy, &key)) {
      logging::Error(
          "SandboxSerializer: could not decode source mode parameter key");
      return false;
    }
    if (!DecodeString(&policy, &value)) {
      logging::Error(
          "SandboxSerializer: could not decode source mode parameter value");
      return false;
    }

    params.push_back(key);
    params.push_back(value);
  }

  std::string error;
  if (!Seatbelt::InitWithParams(profile, 0, params, &error)) {
    logging::Error(
        "SandboxSerializer: Failed to initialize sandbox with params: %s",
        error.c_str());
    return false;
  }

  return true;
}

}  // namespace sandbox
