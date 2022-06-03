// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_pointer_action_list_params.h"

#include "base/check_op.h"

namespace content {

SyntheticPointerActionListParams::SyntheticPointerActionListParams() {}

SyntheticPointerActionListParams::SyntheticPointerActionListParams(
    const ParamList& param_list) {
  params.push_back(param_list);
}

SyntheticPointerActionListParams::SyntheticPointerActionListParams(
    const SyntheticPointerActionListParams& other)
    : SyntheticGestureParams(other), params(other.params) {}

SyntheticPointerActionListParams::~SyntheticPointerActionListParams() {}

SyntheticGestureParams::GestureType
SyntheticPointerActionListParams::GetGestureType() const {
  return POINTER_ACTION_LIST;
}

const SyntheticPointerActionListParams* SyntheticPointerActionListParams::Cast(
    const SyntheticGestureParams* gesture_params) {
  DCHECK(gesture_params);
  DCHECK_EQ(POINTER_ACTION_LIST, gesture_params->GetGestureType());
  return static_cast<const SyntheticPointerActionListParams*>(gesture_params);
}

void SyntheticPointerActionListParams::PushPointerActionParams(
    const SyntheticPointerActionParams& param) {
  ParamList param_list;
  param_list.push_back(param);
  params.push_back(param_list);
}

void SyntheticPointerActionListParams::PushPointerActionParamsList(
    const ParamList& param_list) {
  params.push_back(param_list);
}

}  // namespace content
