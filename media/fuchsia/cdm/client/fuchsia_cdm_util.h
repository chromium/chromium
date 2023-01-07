// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_CLIENT_FUCHSIA_CDM_UTIL_H_
#define MEDIA_FUCHSIA_CDM_CLIENT_FUCHSIA_CDM_UTIL_H_

#include <memory>

namespace blink {
class BrowserInterfaceBrokerProxy;
}

namespace media {
class CdmFactory;

std::unique_ptr<CdmFactory> CreateFuchsiaCdmFactory(
    blink::BrowserInterfaceBrokerProxy* interface_broker);

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_CLIENT_FUCHSIA_CDM_UTIL_H_
