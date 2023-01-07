// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_FACTORY_H_
#define CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_FACTORY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom.h"

namespace content {

// A simple API used by browser interface broker. The main motivation is to
// expose an identical `Create()` interface over different platforms.
void CreateHandwritingRecognitionService(
    mojo::PendingReceiver<handwriting::mojom::HandwritingRecognitionService>);

}  // namespace content

#endif  // CONTENT_BROWSER_HANDWRITING_HANDWRITING_RECOGNITION_SERVICE_FACTORY_H_
