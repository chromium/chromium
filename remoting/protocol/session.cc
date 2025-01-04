// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session.h"

#include "base/location.h"

namespace remoting::protocol {

void Session::Close(ErrorCode error) {
  Close(error, /* error_details= */ {}, /* error_location= */ {});
}

}  // namespace remoting::protocol
