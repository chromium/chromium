// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_RAND_CALLBACK_H_
#define NET_BASE_RAND_CALLBACK_H_

#include "base/callback.h"

namespace net {

typedef base::RepeatingCallback<int(int, int)> RandIntCallback;

}  // namespace net

#endif  // NET_BASE_RAND_CALLBACK_H_
