// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "content/browser/idle/idle_monitor.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "ui/base/idle/idle.h"

namespace content {

IdleMonitor::IdleMonitor(mojo::PendingRemote<blink::mojom::IdleMonitor> monitor,
                         blink::mojom::IdleStatePtr last_state,
                         base::TimeDelta threshold)
    : client_(std::move(monitor)),
      last_state_(std::move(last_state)),
      threshold_(threshold) {}

IdleMonitor::~IdleMonitor() = default;

void IdleMonitor::SetLastState(blink::mojom::IdleStatePtr state) {
  if (!last_state_.Equals(state)) {
    client_->Update(state->Clone());
  }
  last_state_ = std::move(state);
}

void IdleMonitor::SetErrorHandler(
    base::OnceCallback<void(content::IdleMonitor*)> handler) {
  client_.set_connection_error_handler(
      base::BindOnce(std::move(handler), base::Unretained(this)));
}

}  // namespace content
