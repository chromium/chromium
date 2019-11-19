// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IDLE_IDLE_MONITOR_H_
#define CONTENT_BROWSER_IDLE_IDLE_MONITOR_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/linked_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"
#include "ui/base/idle/idle.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT IdleMonitor : public base::LinkNode<IdleMonitor> {
 public:
  IdleMonitor(mojo::PendingRemote<blink::mojom::IdleMonitor> monitor,
              blink::mojom::IdleStatePtr last_state,
              base::TimeDelta threshold);
  ~IdleMonitor();

  const blink::mojom::IdleState& last_state() const {
    return *last_state_.get();
  }
  base::TimeDelta threshold() const { return threshold_; }
  void set_threshold(base::TimeDelta threshold) { threshold_ = threshold; }

  void SetLastState(blink::mojom::IdleStatePtr state);
  void SetErrorHandler(base::OnceCallback<void(content::IdleMonitor*)> handler);

 private:
  blink::mojom::IdleMonitorPtr client_;
  blink::mojom::IdleStatePtr last_state_;
  base::TimeDelta threshold_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(IdleMonitor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_IDLE_IDLE_MONITOR_H_
