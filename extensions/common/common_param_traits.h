// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included
// Multiply-included file, hence no include guard.

#include "components/version_info/channel.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/param_traits_macros.h"

IPC_ENUM_TRAITS_MAX_VALUE(version_info::Channel, version_info::Channel::STABLE)
IPC_ENUM_TRAITS_MAX_VALUE(extensions::mojom::FeatureSessionType,
                          extensions::mojom::FeatureSessionType::kMaxValue)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(
    extensions::mojom::ManifestLocation,
    extensions::mojom::ManifestLocation::kInvalidLocation,
    extensions::mojom::ManifestLocation::kMaxValue)
