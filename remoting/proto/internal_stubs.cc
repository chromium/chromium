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

std::string DoNothingProto::GetTypeName() const {
  return "";
}

// DoNothingProto

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

// ===========================
// RemoteAccessService helpers
// ===========================

// ProvisionCorpMachine

std::string GetMachineProvisioningRequestPath() {
  return "";
}

std::unique_ptr<ProvisionCorpMachineRequest> GetMachineProvisioningRequest(
    const std::string& owner_email,
    const std::string& fqdn,
    const std::string& public_key,
    const std::string& version,
    const std::optional<std::string>& existing_directory_id) {
  return std::make_unique<ProvisionCorpMachineRequest>();
}

const std::string& GetAuthorizationCode(const ProvisionCorpMachineResponse&) {
  return GetEmptyStringRef();
}
const std::string& GetServiceAccount(const ProvisionCorpMachineResponse&) {
  return GetEmptyStringRef();
}
const std::string& GetOwnerEmail(const ProvisionCorpMachineResponse&) {
  return GetEmptyStringRef();
}
const std::string& GetHostId(const ProvisionCorpMachineResponse&) {
  return GetEmptyStringRef();
}

// ReportProvisioningError

std::string GetReportProvisioningErrorRequestPath() {
  return "";
}

std::unique_ptr<ReportProvisioningErrorRequest>
GetReportProvisioningErrorRequest(const std::string& directory_id,
                                  const std::string& error_message,
                                  const std::string& version) {
  return std::make_unique<ReportProvisioningErrorRequest>();
}

// SendHeartbeat

std::string GetSendHeartbeatRequestPath() {
  return "";
}

std::unique_ptr<SendHeartbeatRequest> GetSendHeartbeatRequest(
    const std::string& directory_id) {
  return std::make_unique<SendHeartbeatRequest>();
}

// UpdateRemoteAccessHost

std::string GetUpdateRemoteAccessHostRequestPath() {
  return "";
}

std::unique_ptr<UpdateRemoteAccessHostRequest> GetUpdateRemoteAccessHostRequest(
    const std::string& directory_id,
    std::optional<std::string> host_version,
    std::optional<std::string> signaling_id,
    std::optional<std::string> offline_reason,
    std::optional<std::string> os_name,
    std::optional<std::string> os_version) {
  return std::make_unique<UpdateRemoteAccessHostRequest>();
}

// ===========================
// SessionAuthzService helpers
// ===========================

// GenerateHostToken

std::string GetGenerateHostTokenRequestPath() {
  return "";
}

std::unique_ptr<GenerateHostTokenRequest> GetGenerateHostTokenRequest(
    const GenerateHostTokenRequestStruct&) {
  return std::make_unique<GenerateHostTokenRequest>();
}

std::unique_ptr<GenerateHostTokenResponseStruct>
GetGenerateHostTokenResponseStruct(const GenerateHostTokenResponse&) {
  return std::make_unique<GenerateHostTokenResponseStruct>();
}

// VerifySessionToken

std::string GetVerifySessionTokenRequestPath() {
  return "";
}

std::unique_ptr<VerifySessionTokenRequest> GetVerifySessionTokenRequest(
    const VerifySessionTokenRequestStruct&) {
  return std::make_unique<VerifySessionTokenRequest>();
}

std::unique_ptr<VerifySessionTokenResponseStruct>
GetVerifySessionTokenResponseStruct(const VerifySessionTokenResponse&) {
  return std::make_unique<VerifySessionTokenResponseStruct>();
}

// ReauthorizeHost

std::string GetReauthorizeHostRequestPath() {
  return "";
}

std::unique_ptr<ReauthorizeHostRequest> GetReauthorizeHostRequest(
    const ReauthorizeHostRequestStruct&) {
  return std::make_unique<ReauthorizeHostRequest>();
}

extern std::unique_ptr<ReauthorizeHostResponseStruct>
GetReauthorizeHostResponseStruct(const ReauthorizeHostResponse&) {
  return std::make_unique<ReauthorizeHostResponseStruct>();
}

// ======================
// LoggingService helpers
// ======================

// ReportSessionDisconnected

std::string GetReportSessionDisconnectedRequestPath() {
  return "";
}

std::unique_ptr<ReportSessionDisconnectedRequest>
GetReportSessionDisconnectedRequest(
    const ReportSessionDisconnectedRequestStruct&) {
  return std::make_unique<ReportSessionDisconnectedRequest>();
}

}  // namespace remoting::internal
