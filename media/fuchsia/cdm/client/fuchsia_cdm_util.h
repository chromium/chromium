// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_CLIENT_FUCHSIA_CDM_UTIL_H_
#define MEDIA_FUCHSIA_CDM_CLIENT_FUCHSIA_CDM_UTIL_H_

#include <memory>

namespace service_manager {
class InterfaceProvider;
}

namespace media {
class CdmFactory;

std::unique_ptr<CdmFactory> CreateFuchsiaCdmFactory(
    service_manager::InterfaceProvider* interface_provider);

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_CLIENT_FUCHSIA_CDM_UTIL_H_
