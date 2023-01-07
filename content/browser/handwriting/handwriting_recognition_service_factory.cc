// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/handwriting/handwriting_recognition_service_factory.h"

#include <utility>

#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "content/browser/handwriting/handwriting_recognition_service_impl_cros.h"
#else
// The default service which does not have any real handwriting recognition
// backend.
#include "content/browser/handwriting/handwriting_recognition_service_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace content {

void CreateHandwritingRecognitionService(
    mojo::PendingReceiver<handwriting::mojom::HandwritingRecognitionService>
        pending_receiver) {
#if BUILDFLAG(IS_CHROMEOS)
  CrOSHandwritingRecognitionServiceImpl::Create(std::move(pending_receiver));
#else
  HandwritingRecognitionServiceImpl::Create(std::move(pending_receiver));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace content
