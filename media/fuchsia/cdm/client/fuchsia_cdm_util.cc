// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/client/fuchsia_cdm_util.h"

#include "media/fuchsia/cdm/client/mojo_fuchsia_cdm_provider.h"
#include "media/fuchsia/cdm/fuchsia_cdm_factory.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace media {

std::unique_ptr<CdmFactory> CreateFuchsiaCdmFactory(
    blink::BrowserInterfaceBrokerProxy* interface_broker) {
  return std::make_unique<FuchsiaCdmFactory>(
      std::make_unique<MojoFuchsiaCdmProvider>(interface_broker));
}

}  // namespace media
