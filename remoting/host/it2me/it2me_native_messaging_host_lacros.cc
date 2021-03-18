// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host_lacros.h"

#include <memory>

#include "base/notreached.h"

namespace remoting {

std::unique_ptr<extensions::NativeMessageHost>
CreateIt2MeNativeMessagingHostForLacros(
    scoped_refptr<base::SingleThreadTaskRunner> io_runnner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runnner) {
  // TODO(joedow): Implement a remote support host for LaCrOS.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace remoting
