// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/context_factory.h"

#include "base/check.h"
#include "content/browser/compositor/image_transport_factory.h"

namespace content {

ui::ContextFactory* GetContextFactory() {
  CHECK(ImageTransportFactory::GetInstance(), base::NotFatalUntil::M159);
  return ImageTransportFactory::GetInstance()->GetContextFactory();
}

}  // namespace content
