// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_DELEGATE_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// The delegate receives overscroll gesture updates from the controller and
// should perform appropriate actions. The delegate can optionally cap the
// overscroll deltas maintained and reported by the controller.
class CONTENT_EXPORT OverscrollControllerDelegate {
 public:
  OverscrollControllerDelegate();

  OverscrollControllerDelegate(const OverscrollControllerDelegate&) = delete;
  OverscrollControllerDelegate& operator=(const OverscrollControllerDelegate&) =
      delete;

  virtual ~OverscrollControllerDelegate();

  // Get the size of the display containing the view corresponding to the
  // delegate.
  virtual gfx::Size GetDisplaySize() const = 0;

  // This is called for each update in the overscroll amount. Returns true if
  // the delegate consumed the event.
  virtual bool OnOverscrollUpdate(float delta_x, float delta_y) = 0;

  // This is called when the overscroll completes.
  virtual void OnOverscrollComplete(OverscrollMode overscroll_mode) = 0;

  // This is called when the direction of the overscroll changes. When a new
  // overscroll is started (i.e. when |new_mode| is not equal to
  // OVERSCROLL_NONE), |source| will be set to the device that triggered the
  // overscroll gesture. |behavior| is the value of overscroll-behavior CSS
  // property for the root element.
  virtual void OnOverscrollModeChange(OverscrollMode old_mode,
                                      OverscrollMode new_mode,
                                      OverscrollSource source,
                                      cc::OverscrollBehavior behavior) = 0;

  // Returns the optional maximum amount allowed for the absolute value of
  // overscroll delta corresponding to the current overscroll mode.
  virtual std::optional<float> GetMaxOverscrollDelta() const = 0;

  base::WeakPtr<OverscrollControllerDelegate> GetWeakPtr();

 private:
  base::WeakPtrFactory<OverscrollControllerDelegate> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_DELEGATE_H_
