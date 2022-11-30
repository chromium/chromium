// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/scoped_res_state.h"

#include <cstring>
#include <memory>

#include "base/check.h"
#include "build/build_config.h"

namespace net {

ScopedResState::ScopedResState() {
#if BUILDFLAG(IS_OPENBSD) || BUILDFLAG(IS_FUCHSIA)
  // Note: res_ninit in glibc always returns 0 and sets RES_INIT.
  // res_init behaves the same way.
  memset(&_res, 0, sizeof(_res));
  res_init_result_ = res_init();
#else
  memset(&res_, 0, sizeof(res_));
  res_init_result_ = res_ninit(&res_);
#endif  // BUILDFLAG(IS_OPENBSD) || BUILDFLAG(IS_FUCHSIA)
}

ScopedResState::~ScopedResState() {
#if !BUILDFLAG(IS_OPENBSD) && !BUILDFLAG(IS_FUCHSIA)

  // Prefer res_ndestroy where available.
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FREEBSD)
  res_ndestroy(&res_);
#else
  res_nclose(&res_);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_FREEBSD)

#endif  // !BUILDFLAG(IS_OPENBSD) && !BUILDFLAG(IS_FUCHSIA)
}

bool ScopedResState::IsValid() const {
  return res_init_result_ == 0;
}

const struct __res_state& ScopedResState::state() const {
  DCHECK(IsValid());
#if BUILDFLAG(IS_OPENBSD) || BUILDFLAG(IS_FUCHSIA)
  return _res;
#else
  return res_;
#endif  // BUILDFLAG(IS_OPENBSD) || BUILDFLAG(IS_FUCHSIA)
}

}  // namespace net
