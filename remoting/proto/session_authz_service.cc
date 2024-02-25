// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/proto/session_authz_service.h"

namespace remoting::internal {
GenerateHostTokenResponseStruct::GenerateHostTokenResponseStruct() = default;
GenerateHostTokenResponseStruct::~GenerateHostTokenResponseStruct() = default;

VerifySessionTokenRequestStruct::VerifySessionTokenRequestStruct() = default;
VerifySessionTokenRequestStruct::~VerifySessionTokenRequestStruct() = default;
VerifySessionTokenRequestStruct::VerifySessionTokenRequestStruct(
    const VerifySessionTokenRequestStruct&) = default;
bool VerifySessionTokenRequestStruct::operator==(
    const VerifySessionTokenRequestStruct&) const = default;

VerifySessionTokenResponseStruct::VerifySessionTokenResponseStruct() = default;
VerifySessionTokenResponseStruct::~VerifySessionTokenResponseStruct() = default;

ReauthorizeHostRequestStruct::ReauthorizeHostRequestStruct() = default;
ReauthorizeHostRequestStruct::~ReauthorizeHostRequestStruct() = default;
bool ReauthorizeHostRequestStruct::operator==(
    const ReauthorizeHostRequestStruct&) const = default;

ReauthorizeHostResponseStruct::ReauthorizeHostResponseStruct() = default;
ReauthorizeHostResponseStruct::~ReauthorizeHostResponseStruct() = default;
}  // namespace remoting::internal
