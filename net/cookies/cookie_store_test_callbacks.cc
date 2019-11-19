// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_store_test_callbacks.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/cookies/cookie_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

CookieCallback::CookieCallback(base::Thread* run_in_thread)
    : run_in_thread_(run_in_thread), was_run_(false) {}

CookieCallback::CookieCallback()
    : run_in_thread_(nullptr),
      run_in_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      was_run_(false) {}

CookieCallback::~CookieCallback() = default;

void CookieCallback::ValidateThread() const {
  scoped_refptr<base::SingleThreadTaskRunner> expected_task_runner;
  if (run_in_thread_) {
    DCHECK(!run_in_task_runner_);
    expected_task_runner = run_in_thread_->task_runner();
  } else if (run_in_task_runner_) {
    expected_task_runner = run_in_task_runner_;
  }
  ASSERT_TRUE(expected_task_runner);
  EXPECT_TRUE(expected_task_runner->BelongsToCurrentThread());
}

void CookieCallback::CallbackEpilogue() {
  ValidateThread();
  was_run_ = true;
  loop_to_quit_.Quit();
}

void CookieCallback::WaitUntilDone() {
  loop_to_quit_.Run();
}

bool CookieCallback::was_run() const {
  ValidateThread();
  return was_run_;
}

NoResultCookieCallback::NoResultCookieCallback() = default;
NoResultCookieCallback::NoResultCookieCallback(base::Thread* run_in_thread)
    : CookieCallback(run_in_thread) {}

GetCookieListCallback::GetCookieListCallback() = default;
GetCookieListCallback::GetCookieListCallback(base::Thread* run_in_thread)
    : CookieCallback(run_in_thread) {}

GetCookieListCallback::~GetCookieListCallback() = default;

void GetCookieListCallback::Run(const CookieStatusList& cookies,
                                const CookieStatusList& excluded_cookies) {
  cookies_with_statuses_ = cookies;
  cookies_ = cookie_util::StripStatuses(cookies);
  excluded_cookies_ = excluded_cookies;
  CallbackEpilogue();
}

GetAllCookiesCallback::GetAllCookiesCallback() = default;
GetAllCookiesCallback::GetAllCookiesCallback(base::Thread* run_in_thread)
    : CookieCallback(run_in_thread) {}

GetAllCookiesCallback::~GetAllCookiesCallback() = default;

void GetAllCookiesCallback::Run(const CookieList& cookies) {
  cookies_ = cookies;
  CallbackEpilogue();
}

GetAllCookiesWithAccessSemanticsCallback::
    GetAllCookiesWithAccessSemanticsCallback() = default;
GetAllCookiesWithAccessSemanticsCallback::
    GetAllCookiesWithAccessSemanticsCallback(base::Thread* run_in_thread)
    : CookieCallback(run_in_thread) {}

GetAllCookiesWithAccessSemanticsCallback::
    ~GetAllCookiesWithAccessSemanticsCallback() = default;

void GetAllCookiesWithAccessSemanticsCallback::Run(
    const CookieList& cookies,
    const std::vector<CookieAccessSemantics>& access_semantics_list) {
  cookies_ = cookies;
  access_semantics_list_ = access_semantics_list;
  CallbackEpilogue();
}

}  // namespace net
