// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_SCOPED_RES_STATE_H_
#define NET_DNS_PUBLIC_SCOPED_RES_STATE_H_

#include <resolv.h>

#include <optional>

#include "build/build_config.h"
#include "net/base/net_export.h"

namespace net {

// Helper class to open, read and close a __res_state.
class NET_EXPORT ScopedResState {
 public:
  // This constructor will call memset and res_init/res_ninit a __res_state, and
  // store the result in `res_init_result_`.
  ScopedResState();

  // Calls res_ndestroy or res_nclose if the platform uses `res_`.
  virtual ~ScopedResState();

  // Returns true iff a __res_state was initialized successfully.
  // Other methods in this class shouldn't be called if it returns false.
  bool IsValid() const;

  // Access the __res_state used by this class to compute other values.
  virtual const struct __res_state& state() const;

 private:
#if !BUILDFLAG(IS_OPENBSD) && !BUILDFLAG(IS_FUCHSIA)
  struct __res_state res_;
#endif  // !BUILDFLAG(IS_OPENBSD) && !BUILDFLAG(IS_FUCHSIA)

  int res_init_result_ = -1;
};

}  // namespace net

#endif  // NET_DNS_PUBLIC_SCOPED_RES_STATE_H_
