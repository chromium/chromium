// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/widget_swap_queue.h"

namespace blink {

void WidgetSwapQueue::Queue(int source_frame_number,
                            VisualStateRequestCallback callback,
                            bool* is_first) {
  base::AutoLock lock(lock_);
  if (is_first)
    *is_first = (queue_.count(source_frame_number) == 0);

  queue_[source_frame_number].push_back(std::move(callback));
}

void WidgetSwapQueue::Drain(int source_frame_number) {
  base::AutoLock lock(lock_);
  auto end = queue_.upper_bound(source_frame_number);
  for (auto i = queue_.begin(); i != end; i++) {
    DCHECK(i->first <= source_frame_number);
    std::move(i->second.begin(), i->second.end(),
              std::back_inserter(next_callbacks_));
  }
  queue_.erase(queue_.begin(), end);
}

void WidgetSwapQueue::GetCallbacks(
    Vector<VisualStateRequestCallback>* callbacks) {
  std::move(next_callbacks_.begin(), next_callbacks_.end(),
            std::back_inserter(*callbacks));
  next_callbacks_.clear();
}

}  // namespace blink
