// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/utility/content_utility_client.h"

namespace content {

#if BUILDFLAG(IS_CHROMEOS_ASH)
mojo::GenericPendingReceiver ContentUtilityClient::InitMojoServiceManager() {
  return mojo::GenericPendingReceiver();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace content
