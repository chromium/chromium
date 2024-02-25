// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_SCOPED_MODE_COLLECTION_H_
#define CONTENT_BROWSER_ACCESSIBILITY_SCOPED_MODE_COLLECTION_H_

#include <list>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

class ScopedAccessibilityMode;

// A collection of ScopedAccessibilityMode instances. The collection maintains
// an always up-to-date view of the union of all contained scopers, accessible
// via `accessibility_mode()`. Any change to this value (via calls to `Add()` to
// add a new item to the collection or via destruction of a scoper belonging to
// the collection) results in running the callback provided at construction. It
// is permissible for the collection to be destroyed while scopers minted from
// it remain alive.
class CONTENT_EXPORT ScopedModeCollection {
 public:
  // The type of a callback that is run when the effective mode for the
  // collection changes (i.e., when the union of all mode flags indicated by
  // the scopers in the collection changes).
  using OnModeChangedCallback =
      base::RepeatingCallback<void(ui::AXMode old_mode, ui::AXMode new_mode)>;

  // `on_mode_changed` is run on any change to the collection that results in
  // a different combined accessibility mode.
  explicit ScopedModeCollection(OnModeChangedCallback on_mode_changed);
  ScopedModeCollection(const ScopedModeCollection&) = delete;
  ScopedModeCollection& operator=(const ScopedModeCollection&) = delete;
  ~ScopedModeCollection();

  // Returns the union of all mode flags indicated by the scopers in the
  // collection. Recalculated on each addition, removal, and modification of
  // a scoper in the collection.
  ui::AXMode accessibility_mode() const { return accessibility_mode_; }

  // Returns a new scoper for `mode`, recalculating the effective
  // accessibility mode for the collection and running `on_mode_changed` if it
  // has changed. When the returned scoper is destroyed, the effective
  // accessibility mode for the collection is once again computed and
  // `on_mode_changed` is run if it has changed.
  std::unique_ptr<ScopedAccessibilityMode> Add(ui::AXMode mode);

  // Returns true if the collection is empty.
  bool empty() const { return scopers_.empty(); }

 private:
  class ScopedAccessibilityModeImpl;

  using ScoperContainer = std::list<raw_ptr<ScopedAccessibilityModeImpl>>;
  using ScoperKey = ScoperContainer::iterator;

  // Removes the scoper identified by `scoper_key`, recalculates the effective
  // accessibility mode for the collection, and runs `on_mode_changed` if it
  // has changed.
  void OnDestroyed(ScoperKey scoper_key);

  // Recalculate the effective mode following a change to the collection of
  // scopers. Runs `on_mode_changed` if there is a change.
  void RecalculateEffectiveModeAndNotify();

  // Run on any change to the collection that results in a different combined
  // accessibility mode.
  const OnModeChangedCallback on_mode_changed_;

  // The collection of ScopedAccessibilityMode instances.
  ScoperContainer scopers_;

  // The effective accessibility mode computed from all scopers held by this
  // instance. Recalculated each time a scoper is added, removed, or modified.
  ui::AXMode accessibility_mode_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_SCOPED_MODE_COLLECTION_H_
