// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/scoped_res_state.h"

#include <memory>

#include "base/check.h"

namespace net {

ScopedResState::ScopedResState() {
#if defined(OS_OPENBSD) || defined(OS_FUCHSIA)
  // Note: res_ninit in glibc always returns 0 and sets RES_INIT.
  // res_init behaves the same way.
  memset(&_res, 0, sizeof(_res));
  res_init_result_ = res_init();
#else
  memset(&res_, 0, sizeof(res_));
  res_init_result_ = res_ninit(&res_);
#endif  // defined(OS_OPENBSD) || defined(OS_FUCHSIA)
}

ScopedResState::~ScopedResState() {
#if !defined(OS_OPENBSD) && !defined(OS_FUCHSIA)

  // Prefer res_ndestroy where available.
#if defined(OS_APPLE) || defined(OS_FREEBSD)
  res_ndestroy(&res_);
#else
  res_nclose(&res_);
#endif  // defined(OS_APPLE) || defined(OS_FREEBSD)

#endif  // !defined(OS_OPENBSD) && !defined(OS_FUCHSIA)
}

bool ScopedResState::IsValid() const {
  return res_init_result_ == 0;
}

const struct __res_state& ScopedResState::state() const {
  DCHECK(IsValid());
#if defined(OS_OPENBSD) || defined(OS_FUCHSIA)
  return _res;
#else
  return res_;
#endif  // defined(OS_OPENBSD) || defined(OS_FUCHSIA)
}

}  // namespace net
