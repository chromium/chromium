// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTO_INTERNAL_STUBS_H_
#define REMOTING_PROTO_INTERNAL_STUBS_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "remoting/proto/do_nothing.pb.h"
#include "remoting/proto/logging_service.h"
#include "remoting/proto/messaging_service.h"
#include "remoting/proto/remote_support_service.h"
#include "remoting/proto/session_authz_service.h"

// This file defines proto and function stubs for internal-only implementations.
// This will allow us to build most of our code in Chromium rather than put
// everything in //remoting/internal which is only built on official builders.
namespace remoting::internal {

using DoNothingProto = remoting::DoNothing;

// Aliases for internal protos.
using RemoteAccessHostV1Proto = DoNothingProto;

// ===========================
// RemoteAccessService helpers
// ===========================

// ProvisionCorpMachine
using ProvisionCorpMachineRequest = DoNothingProto;
extern std::string GetMachineProvisioningRequestPath();
extern std::unique_ptr<ProvisionCorpMachineRequest>
GetMachineProvisioningRequest(
    const std::string& owner_email,
    const std::string& fqdn,
    const std::string& public_key,
    const std::string& version,
    const std::optional<std::string>& existing_host_id);

using ProvisionCorpMachineResponse = DoNothingProto;
extern const std::string& GetAuthorizationCode(
    const ProvisionCorpMachineResponse&);
extern const std::string& GetServiceAccount(
    const ProvisionCorpMachineResponse&);
extern const std::string& GetOwnerEmail(const ProvisionCorpMachineResponse&);
extern const std::string& GetHostId(const ProvisionCorpMachineResponse&);

// ReportProvisioningError
using ReportProvisioningErrorRequest = DoNothingProto;
extern std::string GetReportProvisioningErrorRequestPath();
extern std::unique_ptr<ReportProvisioningErrorRequest>
GetReportProvisioningErrorRequest(const std::string& directory_id,
                                  const std::string& error_message,
                                  const std::string& version);

// SendHeartbeat
using SendHeartbeatRequest = DoNothingProto;
extern std::string GetSendHeartbeatRequestPath();
extern std::unique_ptr<SendHeartbeatRequest> GetSendHeartbeatRequest(
    const std::string& directory_id);

// UpdateRemoteAccessHost
using UpdateRemoteAccessHostRequest = DoNothingProto;
extern std::string GetUpdateRemoteAccessHostRequestPath();
extern std::unique_ptr<UpdateRemoteAccessHostRequest>
GetUpdateRemoteAccessHostRequest(const std::string& directory_id,
                                 std::optional<std::string> host_version,
                                 std::optional<std::string> signaling_id,
                                 std::optional<std::string> offline_reason,
                                 std::optional<std::string> os_name,
                                 std::optional<std::string> os_version);

// ===========================
// SessionAuthzService helpers
// ===========================

extern std::string_view GetRemoteAccessSessionAuthzPath();
extern std::string_view GetRemoteSupportSessionAuthzPath();

// GenerateHostToken
using GenerateHostTokenRequest = DoNothingProto;
extern std::string_view GetGenerateHostTokenRequestVerb();
extern std::unique_ptr<GenerateHostTokenRequest> GetGenerateHostTokenRequest(
    const GenerateHostTokenRequestStruct&);

using GenerateHostTokenResponse = DoNothingProto;
extern std::unique_ptr<GenerateHostTokenResponseStruct>
GetGenerateHostTokenResponseStruct(const GenerateHostTokenResponse&);

// VerifySessionToken
using VerifySessionTokenRequest = DoNothingProto;
extern std::string_view GetVerifySessionTokenRequestVerb();
extern std::unique_ptr<VerifySessionTokenRequest> GetVerifySessionTokenRequest(
    const VerifySessionTokenRequestStruct&);

using VerifySessionTokenResponse = DoNothingProto;
extern std::unique_ptr<VerifySessionTokenResponseStruct>
GetVerifySessionTokenResponseStruct(const VerifySessionTokenResponse&);

// ReauthorizeHost
using ReauthorizeHostRequest = DoNothingProto;
extern std::string_view GetReauthorizeHostRequestVerb();
extern std::unique_ptr<ReauthorizeHostRequest> GetReauthorizeHostRequest(
    const ReauthorizeHostRequestStruct&);

using ReauthorizeHostResponse = DoNothingProto;
extern std::unique_ptr<ReauthorizeHostResponseStruct>
GetReauthorizeHostResponseStruct(const ReauthorizeHostResponse&);

// ======================
// LoggingService helpers
// ======================

extern std::string_view GetRemoteAccessLoggingPath();
extern std::string_view GetRemoteSupportLoggingPath();

// ReportSessionDisconnected
extern std::string_view GetReportSessionDisconnectedRequestVerb();
using ReportSessionDisconnectedRequest = DoNothingProto;
extern std::unique_ptr<ReportSessionDisconnectedRequest>
GetReportSessionDisconnectedRequest(
    const ReportSessionDisconnectedRequestStruct&);

// ============================
// RemoteSupportService helpers
// ============================

using RemoteSupportHost = DoNothingProto;
extern std::string_view GetCreateRemoteSupportHostRequestPath();
extern std::unique_ptr<RemoteSupportHost> GetRemoteSupportHost(
    const RemoteSupportHostStruct& request_struct);
extern std::string_view GetSupportId(const RemoteSupportHost&);

// ===========================
// MessagingService helpers
// ===========================

extern std::string_view GetSendHostMessagePath();
extern std::string_view GetReceiveClientMessagesPath();

using ReceiveClientMessagesRequest = DoNothingProto;
extern std::unique_ptr<ReceiveClientMessagesRequest>
GetReceiveClientMessagesRequest(const ReceiveClientMessagesRequestStruct&);

using ReceiveClientMessagesResponse = DoNothingProto;
extern std::unique_ptr<ReceiveClientMessagesResponseStruct>
GetReceiveClientMessagesResponseStruct(const ReceiveClientMessagesResponse&);

using SendHostMessageRequest = DoNothingProto;
extern std::unique_ptr<SendHostMessageRequest> GetSendHostMessageRequest(
    const SendHostMessageRequestStruct&);

using SendHostMessageResponse = DoNothingProto;
extern std::unique_ptr<SendHostMessageResponseStruct>
GetSendHostMessageResponseStruct(const SendHostMessageResponse&);

using SimpleMessage = DoNothingProto;
extern std::unique_ptr<SimpleMessage> GetSimpleMessage(
    const SimpleMessageStruct&);
extern std::unique_ptr<SimpleMessageStruct> GetSimpleMessageStruct(
    const SimpleMessage&);

}  // namespace remoting::internal

#endif  // REMOTING_PROTO_INTERNAL_STUBS_H_
