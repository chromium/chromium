// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_DATA_TYPES_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_DATA_TYPES_H_

#import <optional>
#import <string>
#import <vector>

#import "components/actor/public/mojom/actor_types.mojom.h"

// Represents an incoming TTC actuation request, containing serialized action
// protobufs to execute on controlled WebStates.
struct TtcActuationRequest {
  TtcActuationRequest(std::vector<std::string> action_protos,
                      std::string task_update,
                      std::vector<std::string> call_ids);
  TtcActuationRequest(std::vector<std::string> action_protos,
                      std::string task_update,
                      std::string call_id);
  TtcActuationRequest(const TtcActuationRequest&);
  TtcActuationRequest& operator=(const TtcActuationRequest&);
  TtcActuationRequest(TtcActuationRequest&&);
  TtcActuationRequest& operator=(TtcActuationRequest&&);
  ~TtcActuationRequest();

  // Convenience accessor returning the primary (first) correlation identifier,
  // or `std::nullopt` if `call_ids` is empty.
  std::optional<std::string> call_id() const;

  // Serialized `optimization_guide::proto::Action` protobufs to execute.
  std::vector<std::string> action_protos;

  // Optional progress status message describing the action execution.
  std::string task_update;

  // Correlation identifiers associated with the client tool calls.
  std::vector<std::string> call_ids;
};

// Encapsulates the execution response for a `TtcActuationRequest`.
struct TtcActuationResponse {
  TtcActuationResponse(actor::mojom::ActionResultCode result_code,
                       std::string error_message,
                       std::string call_id);
  TtcActuationResponse(const TtcActuationResponse&);
  TtcActuationResponse& operator=(const TtcActuationResponse&);
  TtcActuationResponse(TtcActuationResponse&&);
  TtcActuationResponse& operator=(TtcActuationResponse&&);
  ~TtcActuationResponse();

  // Execution status code returned by Chrome's Actor Service.
  actor::mojom::ActionResultCode result_code;

  // Descriptive error message if the actuation failed, or empty on success.
  std::string error_message;

  // Correlation identifier matching the request's `call_id`.
  std::string call_id;
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_DATA_TYPES_H_
