// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_TARGET_H_
#define HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_TARGET_H_

#include "headless/public/headless_devtools_channel.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_export.h"

namespace headless {

// A target which can be controlled and inspected using DevTools.
// TODO(dgozman): remove this class once all clients switch.
class HEADLESS_EXPORT HeadlessDevToolsTarget {
 public:
  virtual ~HeadlessDevToolsTarget() {}

  // Attach or detach a client to this target. A client must be attached in
  // order to send commands or receive notifications from the target.
  //
  // A single client may be attached to at most one target at a time.
  // |client| must outlive this target.
  virtual void AttachClient(HeadlessDevToolsClient* client) = 0;
  virtual void DetachClient(HeadlessDevToolsClient* client) = 0;

  // Returns true if a devtools client is attached.
  virtual bool IsAttached() = 0;
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_DEVTOOLS_TARGET_H_
