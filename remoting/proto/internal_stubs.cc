// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/proto/internal_stubs.h"

#include <memory>
#include <string>

namespace remoting::internal {

namespace {
// Rather than using a static string per function or a member in each aliased
// proto, we just use the same empty string here and return it as a const& so
// callers can't modify or mess with it.
const std::string& GetEmptyStringRef() {
  static std::string empty;
  return empty;
}
}  // namespace

const std::string& GetAuthorizationCode(const RemoteAccessHostV1Proto&) {
  return GetEmptyStringRef();
}
const std::string& GetServiceAccount(const RemoteAccessHostV1Proto&) {
  return GetEmptyStringRef();
}
const std::string& GetHostId(const RemoteAccessHostV1Proto&) {
  return GetEmptyStringRef();
}

std::string GetMachineProvisioningRequestPath() {
  return "";
}

std::unique_ptr<RemoteAccessHostV1Proto> GetMachineProvisioningRequest(
    const std::string& owner_email,
    const std::string& fqdn,
    const std::string& public_key,
    absl::optional<std::string> existing_host_id) {
  return std::make_unique<RemoteAccessHostV1Proto>();
}

std::string DoNothingProto::GetTypeName() const {
  return "";
}

google::protobuf::MessageLite* DoNothingProto::New(
    google::protobuf::Arena* arena) const {
  return nullptr;
}

void DoNothingProto::Clear() {}

bool DoNothingProto::IsInitialized() const {
  return true;
}

void DoNothingProto::CheckTypeAndMergeFrom(const MessageLite& other) {}

size_t DoNothingProto::ByteSizeLong() const {
  return 0;
}

int DoNothingProto::GetCachedSize() const {
  return 0;
}

uint8_t* DoNothingProto::_InternalSerialize(
    uint8_t* ptr,
    google::protobuf::io::EpsCopyOutputStream* stream) const {
  return ptr;
}

}  // namespace remoting::internal
