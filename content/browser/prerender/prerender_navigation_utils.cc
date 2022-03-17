// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_navigation_utils.h"

namespace content::prerender_navigation_utils {

// TODO(crbug.com/1299316): Sync with
// https://github.com/WICG/nav-speculation/issues/138 once it's settled down.
bool IsDisallowedHttpResponseCode(int response_code) {
  // Disallow status code 204 and 205 because all error statuses should abandon
  // prerendering as a default behavior.
  if (response_code == 204 || response_code == 205) {
    return true;
  }
  return response_code < 100 || response_code > 399;
}

}  // namespace content::prerender_navigation_utils
