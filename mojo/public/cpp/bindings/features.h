// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_FEATURES_H_
#define MOJO_PUBLIC_CPP_BINDINGS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace mojo {
namespace features {

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) BASE_DECLARE_FEATURE(kTaskPerMessage);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
BASE_DECLARE_FEATURE(kMojoRecordUnreadMessageCount);

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
BASE_DECLARE_FEATURE(kMojoPredictiveAllocation);

}  // namespace features
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_FEATURES_H_
