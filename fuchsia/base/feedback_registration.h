// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_FEEDBACK_REGISTRATION_H_
#define FUCHSIA_BASE_FEEDBACK_REGISTRATION_H_

#include "base/strings/string_piece_forward.h"

namespace cr_fuchsia {

// Overrides the default Fuchsia product info in crash reports.
// Crashes for the component |component_url| will contain |crash_product_name|,
// the version from version_info, and an appropriate value for the release
// channel. |component_url| must match the current component. The calling
// process must have access to "fuchsia.feedback.CrashReportingProductRegister".
void RegisterProductDataForCrashReporting(base::StringPiece component_url,
                                          base::StringPiece crash_product_name);

// Registers basic annotations for the component in |component_namespace|.
// Feedback reports will contain a namespace |component_namespace| that contains
// the version from version_info, and an appropriate value for the release
// channel. The calling process must have access to
// "fuchsia.feedback.ComponentDataRegister".
void RegisterProductDataForFeedback(base::StringPiece component_namespace);

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_FEEDBACK_REGISTRATION_H_