// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_USER_DATA_H_
#define MOJO_CORE_PORTS_USER_DATA_H_

#include "base/memory/ref_counted.h"

namespace mojo {
namespace core {
namespace ports {

class UserData : public base::RefCountedThreadSafe<UserData> {
 protected:
  friend class base::RefCountedThreadSafe<UserData>;

  virtual ~UserData() = default;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_USER_DATA_H_
