// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/message_from_service_worker.h"

#include <utility>

namespace blink {

MessageFromServiceWorker::MessageFromServiceWorker(
    WebServiceWorkerObjectInfo source,
    blink::TransferableMessage message)
    : source(std::move(source)), message(std::move(message)) {}

MessageFromServiceWorker::~MessageFromServiceWorker() = default;

}  // namespace blink
