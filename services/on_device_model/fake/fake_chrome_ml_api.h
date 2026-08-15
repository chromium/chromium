// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_FAKE_FAKE_CHROME_ML_API_H_
#define SERVICES_ON_DEVICE_MODEL_FAKE_FAKE_CHROME_ML_API_H_

#include "base/auto_reset.h"
#include "base/component_export.h"
#include "services/on_device_model/ml/chrome_ml_api.h"

namespace fake_ml {

// Fake tool call values returned by the fake ChromeML API.
inline constexpr char kFakeToolCallId[] = "fake_call_1";
inline constexpr char kFakeToolName[] = "fake_tool";

// Format prefixes used by PieceToString for tool-related input pieces.
inline constexpr char kToolDeclPrefix[] = "[ToolDecl:";
inline constexpr char kToolRespPrefix[] = "[ToolResp:";

const ChromeMLAPI* GetFakeMlApi();

// TODO(crbug.com/540118700): Remove once the legacy engine has been removed and
// all of these unittests use context_usage by default.
//
// Enables calculating and setting tokens_decoded in the fake ChromeML API
// during a test, resetting to its previous value when the returned AutoReset is
// destroyed.
COMPONENT_EXPORT(ON_DEVICE_MODEL_FAKE)
base::AutoReset<bool> EnableCalculateTokensDecodedForTesting();

}  // namespace fake_ml

#endif  // SERVICES_ON_DEVICE_MODEL_FAKE_FAKE_CHROME_ML_API_H_
