// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"

#import <utility>

TtcActuationRequest::TtcActuationRequest(std::vector<std::string> action_protos,
                                         std::string task_update)
    : action_protos(std::move(action_protos)),
      task_update(std::move(task_update)) {}

TtcActuationRequest::TtcActuationRequest(const TtcActuationRequest&) = default;
TtcActuationRequest& TtcActuationRequest::operator=(
    const TtcActuationRequest&) = default;
TtcActuationRequest::TtcActuationRequest(TtcActuationRequest&&) = default;
TtcActuationRequest& TtcActuationRequest::operator=(TtcActuationRequest&&) =
    default;
TtcActuationRequest::~TtcActuationRequest() = default;

TtcActuationResponse::TtcActuationResponse(
    actor::mojom::ActionResultCode result_code,
    std::string error_message)
    : result_code(result_code), error_message(std::move(error_message)) {}

TtcActuationResponse::TtcActuationResponse(const TtcActuationResponse&) =
    default;
TtcActuationResponse& TtcActuationResponse::operator=(
    const TtcActuationResponse&) = default;
TtcActuationResponse::TtcActuationResponse(TtcActuationResponse&&) = default;
TtcActuationResponse& TtcActuationResponse::operator=(TtcActuationResponse&&) =
    default;
TtcActuationResponse::~TtcActuationResponse() = default;
