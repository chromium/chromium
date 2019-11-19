// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/task/task_traits.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

#if defined(NCTEST_BROWSER_TASK_TRAITS_NO_THREAD)  // [r"Either content::BrowserThread::ID or base::CurrentThread must be set, but not both"]
constexpr base::TaskTraits traits = {BrowserTaskType::kNavigation};
#elif defined(NCTEST_BROWSER_TASK_TRAITS_MULTIPLE_THREADS)  // [r"The traits bag contains multiple traits of the same type."]
constexpr base::TaskTraits traits = {BrowserThread::UI,
                                     BrowserThread::IO};
#elif defined(NCTEST_BROWSER_TASK_TRAITS_BROWSER_THREAD_AND_CURRENT_THREAD)  // [r"Either content::BrowserThread::ID or base::CurrentThread must be set, but not both"]
constexpr base::TaskTraits traits = {BrowserThread::UI, base::CurrentThread()};
#endif

}  // namespace content
