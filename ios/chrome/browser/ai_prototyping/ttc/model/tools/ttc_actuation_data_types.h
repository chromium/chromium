// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_DATA_TYPES_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_DATA_TYPES_H_

#import <string>
#import <vector>

// Represents an incoming TTC actuation request, containing serialized action
// protobufs to execute on controlled WebStates.
struct TtcActuationRequest {
  TtcActuationRequest(std::vector<std::string> action_protos,
                      std::string task_update);
  TtcActuationRequest(const TtcActuationRequest&);
  TtcActuationRequest& operator=(const TtcActuationRequest&);
  TtcActuationRequest(TtcActuationRequest&&);
  TtcActuationRequest& operator=(TtcActuationRequest&&);
  ~TtcActuationRequest();

  // Serialized `optimization_guide::proto::Action` protobufs to execute.
  std::vector<std::string> action_protos;

  // Optional progress status message describing the action execution.
  std::string task_update;
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_DATA_TYPES_H_
