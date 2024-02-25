// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/scoped_mode_collection.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "content/public/browser/scoped_accessibility_mode.h"

namespace content {

// A concrete ScopedAccessibilityMode that belongs to a `ScopedModeCollection`.
// Instances remove themselves from their collection if they are destroyed
// before the collection itself. Instances may outlive their collection, in
// which case the collection deactivates them so that they do nothing when
// destroyed.
class ScopedModeCollection::ScopedAccessibilityModeImpl
    : public ScopedAccessibilityMode {
 public:
  ScopedAccessibilityModeImpl(ui::AXMode mode,
                              ScopedModeCollection* owner,
                              ScoperKey key)
      : ScopedAccessibilityMode(mode), owner_(owner), key_(key) {}
  ScopedAccessibilityModeImpl(const ScopedAccessibilityModeImpl&) = delete;
  ScopedAccessibilityModeImpl& operator=(const ScopedAccessibilityModeImpl&) =
      delete;
  ~ScopedAccessibilityModeImpl() override {
    if (owner_) {
      // This scoper is being destroyed before its owner; notify the owner so
      // that it can remove it from the collection, recalculate its effective
      // accessibility mode, and notify as appropriate. Take ownership of the
      // pointer to the owner since it may self-destruct.
      ScopedModeCollection* owner = std::exchange(owner_, nullptr);
      owner->OnDestroyed(std::exchange(key_, ScoperKey()));
    }
  }

  void deactivate() {
    owner_ = nullptr;
    key_ = ScoperKey();
  }

 private:
  raw_ptr<ScopedModeCollection> owner_;
  ScoperKey key_;
};

ScopedModeCollection::ScopedModeCollection(
    OnModeChangedCallback on_mode_changed)
    : on_mode_changed_(std::move(on_mode_changed)) {}

ScopedModeCollection::~ScopedModeCollection() {
  // The target to which this collection applies is being destroyed. It is valid
  // for this to happen before all scopers have been destroyed (e.g., if both
  // the collection and a scoper are bound to the lifetime of the target and the
  // collection happens to be destroyed first). In this case, deactivate any
  // remaining scopers so that they do nothing when they are later destroyed.
  base::ranges::for_each(scopers_, [](auto& scoper) { scoper->deactivate(); });
}

std::unique_ptr<ScopedAccessibilityMode> ScopedModeCollection::Add(
    ui::AXMode mode) {
  // Add an item to the list for the new scoper and grab an iterator to it.
  scopers_.push_back(nullptr);
  auto iter = --scopers_.end();

  // Make the scoper and put it into the collection at the location just created
  // for it.
  auto scoper = std::make_unique<ScopedAccessibilityModeImpl>(mode, this, iter);
  *iter = scoper.get();

  RecalculateEffectiveModeAndNotify();

  return scoper;
}

void ScopedModeCollection::OnDestroyed(ScoperKey scoper_key) {
  scopers_.erase(scoper_key);

  RecalculateEffectiveModeAndNotify();
}

void ScopedModeCollection::RecalculateEffectiveModeAndNotify() {
  ui::AXMode mode;

  base::ranges::for_each(
      scopers_, [&mode](const auto& scoper) { mode |= scoper->mode(); });

  if (mode == accessibility_mode_) {
    return;
  }

  // Run a copy of the callback in case running it deletes `this`.
  auto callback_copy = on_mode_changed_;
  std::move(callback_copy).Run(std::exchange(accessibility_mode_, mode), mode);
}

}  // namespace content
