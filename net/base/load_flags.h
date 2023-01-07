// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_FLAGS_H_
#define NET_BASE_LOAD_FLAGS_H_

namespace net {

// These flags provide metadata about the type of the load request.  They are
// intended to be OR'd together.
enum {

#define LOAD_FLAG(label, value) LOAD_ ## label = value,
#include "net/base/load_flags_list.h"
#undef LOAD_FLAG

};

}  // namespace net

#endif  // NET_BASE_LOAD_FLAGS_H_
