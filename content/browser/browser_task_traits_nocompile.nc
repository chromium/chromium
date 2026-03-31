// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "content/public/browser/browser_task_traits.h"

namespace content {

// expected-error@base/traits_bag.h:* {{static assertion failed: The traits bag contains multiple traits of the same type.}}
constexpr BrowserTaskTraits traits = {BrowserTaskType::kNavigationNetworkResponse, BrowserTaskType::kUserInput};

}  // namespace content
