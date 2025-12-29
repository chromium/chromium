// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/actuation_action_factory.h"

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actions/actuation_command.h"

ActuationActionFactory::ActuationActionFactory() = default;
ActuationActionFactory::~ActuationActionFactory() = default;

std::unique_ptr<ActuationCommand> ActuationActionFactory::CreateActionCommand(
    const optimization_guide::proto::Action& action) {
  // TODO(crbug.com/472291687): Implement command creation logic using the
  // proto based on the type.
  return nullptr;
}
