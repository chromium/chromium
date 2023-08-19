// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_SESSION_ID_H_
#define REMOTING_HOST_CHROMEOS_SESSION_ID_H_

#include "base/types/strong_alias.h"

namespace remoting {

using SessionId = base::StrongAlias<class SessionIdTag, int>;

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_SESSION_ID_H_
