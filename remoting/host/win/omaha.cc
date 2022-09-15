// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/omaha.h"

namespace remoting {

// The Omaha Appid of the host. It should be kept in sync with $(var.OmahaAppid)
// defined in remoting/host/win/chromoting.wxs and the Omaha server
// configuration.
const wchar_t kHostOmahaAppid[] = L"{b210701e-ffc4-49e3-932b-370728c72662}";

}  // namespace remoting
