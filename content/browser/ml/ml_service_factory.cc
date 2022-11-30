// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ml/ml_service_factory.h"

#include <utility>

#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/browser/ml/ml_service_impl_cros.h"
#else
// The default service which does not have any real backend.
#include "content/browser/ml/ml_service_impl.h"
#endif  // IS_CHROMEOS_ASH

namespace content {

void CreateMLService(mojo::PendingReceiver<ml::model_loader::mojom::MLService>
                         pending_receiver) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CrOSMLServiceImpl::Create(std::move(pending_receiver));
#else
  MLServiceImpl::Create(std::move(pending_receiver));
#endif
}

}  // namespace content
