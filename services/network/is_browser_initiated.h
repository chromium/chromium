// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IS_BROWSER_INITIATED_H_
#define SERVICES_NETWORK_IS_BROWSER_INITIATED_H_

#include "base/types/strong_alias.h"

namespace network {

// A boolean-like type to represent whether a URLLoaderFactory is created for
// the browser process. This is naturally extended for contexts attached to
// URLLoaderFactories. See URLLoaderFactoryParams.process_id for details.
using IsBrowserInitiated = base::StrongAlias<class IsBrowserInitiatedTag, bool>;

}  // namespace network

#endif  // SERVICES_NETWORK_IS_BROWSER_INITIATED_H_
