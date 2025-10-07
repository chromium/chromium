// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_SENDER_H_
#define IPC_IPC_SENDER_H_

#include "base/component_export.h"

namespace IPC {

class COMPONENT_EXPORT(IPC) Sender {
 protected:
  virtual ~Sender() {}
};

}  // namespace IPC

#endif  // IPC_IPC_SENDER_H_
