// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTO_INTERNAL_STUBS_H_
#define REMOTING_PROTO_INTERNAL_STUBS_H_

#include <memory>
#include <string>

#include <optional>
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

// This file defines proto and function stubs for internal-only implementations.
// This will allow us to build most of our code in Chromium rather than put
// everything in //remoting/internal which is only built on official builders.
namespace remoting::internal {

// Base proto used for aliasing.
class DoNothingProto : public google::protobuf::MessageLite {
 public:
  // google::protobuf::MessageLite
  std::string GetTypeName() const override;
  MessageLite* New(google::protobuf::Arena* arena) const override;
  void Clear() override;
  bool IsInitialized() const override;
  void CheckTypeAndMergeFrom(const MessageLite& other) override;
  size_t ByteSizeLong() const override;
  int GetCachedSize() const override;
  uint8_t* _InternalSerialize(
      uint8_t* ptr,
      google::protobuf::io::EpsCopyOutputStream* stream) const override;
};

// Aliases for internal protos.
using ProvisionCorpMachineRequest = DoNothingProto;
using ProvisionCorpMachineResponse = DoNothingProto;
using RemoteAccessHostV1Proto = DoNothingProto;
using ReportProvisioningErrorRequest = DoNothingProto;
using SendHeartbeatRequest = DoNothingProto;

// RemoteAccessHost helpers.
extern const std::string& GetAuthorizationCode(
    const ProvisionCorpMachineResponse&);
extern const std::string& GetServiceAccount(
    const ProvisionCorpMachineResponse&);
extern const std::string& GetOwnerEmail(const ProvisionCorpMachineResponse&);
extern const std::string& GetHostId(const ProvisionCorpMachineResponse&);

// RemoteAccessService helpers.
extern std::string GetMachineProvisioningRequestPath();
extern std::unique_ptr<ProvisionCorpMachineRequest>
GetMachineProvisioningRequest(const std::string& owner_email,
                              const std::string& fqdn,
                              const std::string& public_key,
                              const std::string& version,
                              std::optional<std::string> existing_host_id);

extern std::string GetReportProvisioningErrorRequestPath();
extern std::unique_ptr<ReportProvisioningErrorRequest>
GetReportProvisioningErrorRequest(const std::string& host_id,
                                  const std::string& error_message,
                                  const std::string& version);

extern std::string GetSendHeartbeatRequestPath();
extern std::unique_ptr<SendHeartbeatRequest> GetSendHeartbeatRequest(
    const std::string& host_id);

}  // namespace remoting::internal

#endif  // REMOTING_PROTO_INTERNAL_STUBS_H_
