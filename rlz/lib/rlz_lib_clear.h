// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_LIB_RLZ_LIB_CLEAR_H_
#define RLZ_LIB_RLZ_LIB_CLEAR_H_

#include "rlz/lib/rlz_api.h"
#include "rlz/lib/rlz_enums.h"

namespace rlz_lib {

// Clears all product-specifc state from the RLZ registry.
// Should be called during product uninstallation.
// This removes outstanding product events, product financial ping times,
// the product RLS argument (if any), and any RLZ's for access points being
// uninstalled with the product.
// access_points is an array terminated with NO_ACCESS_POINT.
// IMPORTANT: These are the access_points the product is removing as part
// of the uninstallation, not necessarily all the access points passed to
// SendFinancialPing() and GetPingParams().
// access_points can be null if no points are being uninstalled.
// No return value - this is best effort. Will assert in debug mode on
// failed attempts.
// Access: HKCU write.
void RLZ_LIB_API ClearProductState(Product product,
                                   const AccessPoint* access_points);

}  // namespace rlz_lib

#endif  // RLZ_LIB_RLZ_LIB_CLEAR_H_
