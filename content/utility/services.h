// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_SERVICES_H_
#define CONTENT_UTILITY_SERVICES_H_

#include "content/common/content_export.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace mojo {
class ServiceFactory;
}

namespace content {
using NetworkBinderCreationCallback =
    base::OnceCallback<void(service_manager::BinderRegistry*)>;
CONTENT_EXPORT void SetNetworkBinderCreationCallbackForTesting(  // IN-TEST
    NetworkBinderCreationCallback callback);

void RegisterIOThreadServices(mojo::ServiceFactory& services);
void RegisterMainThreadServices(mojo::ServiceFactory& services);

}  // namespace content

#endif  // CONTENT_UTILITY_SERVICES_H_
