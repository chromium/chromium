// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_SERVICES_H_
#define CONTENT_UTILITY_SERVICES_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

namespace content {

void HandleServiceRequestOnIOThread(
    mojo::GenericPendingReceiver receiver,
    base::SequencedTaskRunner* main_thread_task_runner);

void HandleServiceRequestOnMainThread(mojo::GenericPendingReceiver receiver);

}  // namespace content

#endif  // CONTENT_UTILITY_SERVICES_H_
