// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_OPERATION_H_
#define DEVICE_FIDO_MAC_OPERATION_H_

namespace device {
namespace fido {
namespace mac {

class Operation {
 public:
  Operation() = default;

  Operation(const Operation&) = delete;
  Operation& operator=(const Operation&) = delete;

  virtual ~Operation() = default;
  virtual void Run() = 0;
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_OPERATION_H_
