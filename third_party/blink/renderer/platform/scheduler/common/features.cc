// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/features.h"

namespace blink {
namespace scheduler {

// Field trial parameters associated with |kThrottleAndFreezeTaskTypes| feature.
// These override the throttleable and freezable QueueTraits bits for the tasks
// specified in the parameters. The parameters are comma separated lists, of the
// the form: "throttleable": "name1,name2", "freezable": "name1,name3". The
// names should be those returned by TaskTypeNames::TaskTypeToString().
const char kThrottleableTaskTypesListParam[] = "ThrottleableTasks";
const char kFreezableTaskTypesListParam[] = "FreezableTasks";

}  // namespace scheduler
}  // namespace blink
