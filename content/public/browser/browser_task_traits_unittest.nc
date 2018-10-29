// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/task/task_traits.h"
#include "content/public/browser/browser_task_traits.h"

namespace content {

#if defined(NCTEST_BROWSER_TASK_TRAITS_NO_THREAD)  // [r"TaskTraits contains a Trait that must be explicity initialized in its constructor."]
constexpr base::TaskTraits traits = {NonNestable()};
#elif defined(NCTEST_BROWSER_TASK_TRAITS_MULTIPLE_THREADS)  // [r"Multiple arguments of the same type were provided to the constructor of TaskTraits."]
constexpr base::TaskTraits traits = {BrowserThread::UI,
                                     BrowserThread::IO};
#endif

}  // namespace content
