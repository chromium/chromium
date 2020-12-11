// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MOJO_BINDING_CONTEXT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MOJO_BINDING_CONTEXT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_common.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class BrowserInterfaceBrokerProxy;

// This class encapsulates the necessary information for binding Mojo
// interfaces, to enable interfaces provided by the platform to be aware of the
// context in which they are intended to be used.
class BLINK_PLATFORM_EXPORT MojoBindingContext {
 public:
  virtual const BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker()
      const = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MOJO_BINDING_CONTEXT_H_
