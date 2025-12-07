// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_SCOPED_MODE_COLLECTION_H_
#define CONTENT_BROWSER_ACCESSIBILITY_SCOPED_MODE_COLLECTION_H_

#include <list>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_mode.h"

namespace content {

class ScopedAccessibilityMode;

// A collection of ScopedAccessibilityMode instances. The collection maintains
// an always up-to-date view of the union of all contained scopers, accessible
// via `accessibility_mode()`. Any change to this value (via calls to `Add()` to
// add a new item to the collection or via destruction of a scoper belonging to
// the collection) results in notifying the delegate. The delegate may filter
// mode flags during recomputation of the effective mode. It is permissible for
// the collection to be destroyed while scopers minted from it remain alive.
class CONTENT_EXPORT ScopedModeCollection {
 public:
  class Delegate {
   public:
    // Called when the effective mode for the collection changes (i.e., when the
    // union of all mode flags indicated by the scopers in the collection
    // changes).
    virtual void OnModeChanged(ui::AXMode old_mode, ui::AXMode new_mode) = 0;

    // Filters `mode`, returning some subset of `mode`. Called once for each
    // scoper in the collection while computing the collection's effective mode.
    virtual ui::AXMode FilterModeFlags(ui::AXMode mode) = 0;

   protected:
    Delegate() = default;
    ~Delegate() = default;

    // Returns a PassKey for use by the Delegate so that it may force
    // recomputation if its filtering policy changes.
    static base::PassKey<Delegate> MakePassKey() {
      return base::PassKey<Delegate>();
    }
  };

  explicit ScopedModeCollection(Delegate& delegate);
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

  // Forces a recomputation of the collection's effective mode. To be called by
  // the delegate when the behavior of the delegate's filter function changes.
  void Recompute(base::PassKey<Delegate>);

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

  const raw_ref<Delegate> delegate_;

  // The collection of ScopedAccessibilityMode instances.
  ScoperContainer scopers_;

  // The effective accessibility mode computed from all scopers held by this
  // instance. Recalculated each time a scoper is added, removed, or modified.
  ui::AXMode accessibility_mode_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_SCOPED_MODE_COLLECTION_H_
