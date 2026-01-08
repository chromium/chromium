// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_ACTUATION_UTIL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_ACTUATION_UTIL_H_

#include <optional>
#include <string>

#import "components/optimization_guide/proto/features/actions_data.pb.h"

// Returns the string representation of the given Actuation tool if supported.
// This is used for mapping the enum to the "DisabledTools" feature parameter.
std::optional<std::string> ActuationActionCaseToToolName(
    optimization_guide::proto::Action::ActionCase tool);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_ACTUATION_UTIL_H_
