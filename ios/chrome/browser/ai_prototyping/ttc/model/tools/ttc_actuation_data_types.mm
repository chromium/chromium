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
