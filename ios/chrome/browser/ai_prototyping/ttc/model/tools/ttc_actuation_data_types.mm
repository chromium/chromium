// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"

#import <optional>
#import <utility>

TtcActuationRequest::TtcActuationRequest(std::vector<std::string> action_protos,
                                         std::string task_update,
                                         std::vector<std::string> call_ids)
    : action_protos(std::move(action_protos)),
      task_update(std::move(task_update)),
      call_ids(std::move(call_ids)) {}

TtcActuationRequest::TtcActuationRequest(std::vector<std::string> action_protos,
                                         std::string task_update,
                                         std::string call_id)
    : action_protos(std::move(action_protos)),
      task_update(std::move(task_update)) {
  if (!call_id.empty()) {
    call_ids.push_back(std::move(call_id));
  }
}

TtcActuationRequest::TtcActuationRequest(const TtcActuationRequest&) = default;
TtcActuationRequest& TtcActuationRequest::operator=(
    const TtcActuationRequest&) = default;
TtcActuationRequest::TtcActuationRequest(TtcActuationRequest&&) = default;
TtcActuationRequest& TtcActuationRequest::operator=(TtcActuationRequest&&) =
    default;
TtcActuationRequest::~TtcActuationRequest() = default;

std::optional<std::string> TtcActuationRequest::call_id() const {
  if (call_ids.empty()) {
    return std::nullopt;
  }
  return call_ids.front();
}

TtcActuationResponse::TtcActuationResponse(
    actor::mojom::ActionResultCode result_code,
    std::string error_message,
    std::string call_id)
    : result_code(result_code),
      error_message(std::move(error_message)),
      call_id(std::move(call_id)) {}

TtcActuationResponse::TtcActuationResponse(const TtcActuationResponse&) =
    default;
TtcActuationResponse& TtcActuationResponse::operator=(
    const TtcActuationResponse&) = default;
TtcActuationResponse::TtcActuationResponse(TtcActuationResponse&&) = default;
TtcActuationResponse& TtcActuationResponse::operator=(TtcActuationResponse&&) =
    default;
TtcActuationResponse::~TtcActuationResponse() = default;
