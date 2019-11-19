// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_FRAME_TEST_UTIL_H_
#define FUCHSIA_BASE_FRAME_TEST_UTIL_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/values.h"

namespace cr_fuchsia {

// Uses |navigation_controller| to load |url| with |load_url_params|. Returns
// after the load is completed. Returns true if the load was successful, false
// otherwise.
bool LoadUrlAndExpectResponse(
    fuchsia::web::NavigationController* navigation_controller,
    fuchsia::web::LoadUrlParams load_url_params,
    base::StringPiece url);

// Executes |script| in the context of |frame|'s top-level document.
// Returns an un-set |base::Optional<>| on failure.
base::Optional<base::Value> ExecuteJavaScript(fuchsia::web::Frame* frame,
                                              base::StringPiece script);

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_FRAME_TEST_UTIL_H_
