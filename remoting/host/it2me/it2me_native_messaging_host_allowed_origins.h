// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_ALLOWED_ORIGINS_H_
#define REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_ALLOWED_ORIGINS_H_

#include <stddef.h>

namespace remoting {

// The set of origins which are allowed to instantiate an It2Me host.
extern const char* const kIt2MeOrigins[];

// The number of entries defined in |kIt2MeOrigins|.
extern const size_t kIt2MeOriginsSize;

// The name used to register the It2Me native message host.
extern const char kIt2MeNativeMessageHostName[];

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_ALLOWED_ORIGINS_H_
