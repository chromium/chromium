// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_LACROS_H_
#define REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_LACROS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "extensions/browser/api/messaging/native_message_host.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// Creates native messaging host on Lacros. Must be called on the UI thread of
// the browser process.

std::unique_ptr<extensions::NativeMessageHost>
CreateIt2MeNativeMessagingHostForLacros(
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner);

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_LACROS_H_
