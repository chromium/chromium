// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_executable/service_main.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"

void ServiceMain(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {}
