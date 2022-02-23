// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CURTAIN_MODE_H_
#define REMOTING_HOST_CURTAIN_MODE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ClientSessionControl;

class CurtainMode {
 public:
  CurtainMode(const CurtainMode&) = delete;
  CurtainMode& operator=(const CurtainMode&) = delete;

  virtual ~CurtainMode() {}

  // Creates a platform-specific curtain mode implementation object that
  // "curtains" the current session making sure it is not accessible from
  // the local console. |client_session_control| can be used to drop
  // the connection in the case if the session re-connects to the local console
  // in mid-flight.
  static std::unique_ptr<CurtainMode> Create(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);

  // Activates the curtain mode. Returns true if successful.
  virtual bool Activate() = 0;

 protected:
  CurtainMode() {}
};

}  // namespace remoting

#endif  // REMOTING_HOST_CURTAIN_MODE_H_
