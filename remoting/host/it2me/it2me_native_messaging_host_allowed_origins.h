// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_ALLOWED_ORIGINS_H_
#define REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_ALLOWED_ORIGINS_H_

#include <stddef.h>

namespace remoting {

// The set of origins which are allowed to instantiate an It2Me host.
// LINT.IfChange(it2me_origins)
inline constexpr const char* kIt2MeOrigins[] = {
    "chrome-extension://inomeogfingihgjfjlpeplalcfajhgai/",
    "chrome-extension://pbnaomcgbfiofkfobmlhmdobjchjkphi/"};
// LINT.ThenChange(/remoting/host/BUILD.gn:extension_ids)

// The name used to register the It2Me native message host.
inline constexpr char kIt2MeNativeMessageHostName[] =
    "com.google.chrome.remote_assistance";

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_ALLOWED_ORIGINS_H_
