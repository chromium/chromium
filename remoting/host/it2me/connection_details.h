// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_CONNECTION_DETAILS_H_
#define REMOTING_HOST_IT2ME_CONNECTION_DETAILS_H_

#include <string>

namespace remoting {

struct ConnectionDetails {
  std::string remote_username;
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_CONNECTION_DETAILS_H_
