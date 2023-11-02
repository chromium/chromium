// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_UTILITY_GRAPHICS_PAINT_MANAGER_H_
#define PPAPI_UTILITY_GRAPHICS_PAINT_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/graphics/paint_aggregator.h"

/// @file
/// This file defines the API to convert the "plugin push" model of painting
/// in PPAPI to a paint request at a later time.

namespace pp {

class Graphics2D;
class Instance;
class Point;
class Rect;

/// This class converts the "instance push" model of painting in PPAPI to a
/// paint request at a later time. Usage is that you call Invalidate and
/// Scroll, and implement the Client interface. Your OnPaint handler will
/// then get called with coalesced paint events.
///
/// This class is basically a <code>PaintAggregator</code> that groups updates,
/// plus management of callbacks for scheduling paints.
///
/// <strong>Example:</strong>
///
/// <code>
///
///  class MyClass : public pp::Instance, public PaintManager::Client {
///   public:
///    MyClass() {
///      paint_manager_.Initialize(this, this, false);
///    }
///
///    void ViewChanged(const pp::Rect& position, const pp::Rect& clip) {
///      paint_manager_.SetSize(position.size());
///    }
///
///    void DoSomething() {
///      // This function does something like respond to an event that causes
///      // the screen to need updating.
///      paint_manager_.InvalidateRect(some_rect);
///    }
///
///    // Implementation of PaintManager::Client
///    virtual bool OnPaint(pp::Graphics2D& device,
///                         const pp::PaintUpdate& update) {
///      // If our app needed scrolling, we would apply that first here.
///
///      // Then we would either repaint the area returned by GetPaintBounds or
///      // iterate through all the paint_rects.
///
///      // The caller will call Flush() for us, so don't do that here.
///      return true;
///    }
///
///   private:
///    pp::PaintManager paint_manager_;
///  };
/// </code>
class PaintManager {
 public:
  class Client {
   public:
    /// OnPaint() paints the given invalid area of the instance to the given
    /// graphics device. Returns true if anything was painted.
    ///
    /// You are given the list of rects to paint in <code>paint_rects</code>,
    /// and the union of all of these rects in <code>paint_bounds</code>. You
    /// only have to paint the area inside each of the
    /// <code>paint_rects</code>, but can paint more if you want (some apps may
    /// just want to paint the union).
    ///
    /// Do not call Flush() on the graphics device, this will be done
    /// automatically if you return true from this function since the
    /// <code>PaintManager</code> needs to handle the callback.
    ///
    /// It is legal for you to cause invalidates inside of Paint which will
    /// then get executed as soon as the Flush for this update has completed.
    /// However, this is not very nice to the host system since it will spin the
    /// CPU, possibly updating much faster than necessary. It is best to have a
    /// 1/60 second timer to do an invalidate instead. This will limit your
    /// animation to the slower of 60Hz or "however fast Flush can complete."
    ///
    /// @param[in] graphics A <code>Graphics2D</code> to be painted.
    /// @param[in] paint_rects A list of rects to paint.
    /// @param[in] paint_bounds A union of the rects to paint.
    ///
    /// @return true if successful, otherwise false.
    virtual bool OnPaint(Graphics2D& graphics,
                         const std::vector<Rect>& paint_rects,
                         const Rect& paint_bounds) = 0;

   protected:
    // You shouldn't be doing deleting through this interface.
    virtual ~Client() {}
  };

  /// Default constructor for creating an is_null() <code>PaintManager</code>
  /// object. If you use this version of the constructor, you must call
  /// Initialize() below.
  PaintManager();

  /// A constructor to create a new <code>PaintManager</code> with an instance
  /// and client.
  ///
  /// <strong>Note:</strong> You will need to call SetSize() before this class
  /// will do anything. Normally you do this from the <code>ViewChanged</code>
  /// method of your instance.
  ///
  /// @param instance The instance using this paint manager to do its
  /// painting. Painting will automatically go to this instance and you don't
  /// have to manually bind any device context (this is all handled by the
  /// paint manager).
  ///
  /// @param client A non-owning pointer and must remain valid (normally the
  /// object implementing the Client interface will own the paint manager).
  ///
  /// @param is_always_opaque A flag passed to the device contexts that this
  /// class creates. Set this to true if your instance always draws an opaque
  /// image to the device. This is used as a hint to the browser that it does
  /// not need to do alpha blending, which speeds up painting. If you generate
  /// non-opqaue pixels or aren't sure, set this to false for more general
  /// blending.
  ///
  /// If you set is_always_opaque, your alpha channel should always be set to
  /// 0xFF or there may be painting artifacts. Being opaque will allow the
  /// browser to do a memcpy rather than a blend to paint the plugin, and this
  /// means your alpha values will get set on the page backing store. If these
  /// values are incorrect, it could mess up future blending. If you aren't
  /// sure, it is always correct to specify that it it not opaque.
  PaintManager(Instance* instance, Client* client, bool is_always_opaque);

  /// Destructor.
  ~PaintManager();

  /// Initialize() must be called if you are using the 0-arg constructor.
  ///
  /// @param instance The instance using this paint manager to do its
  /// painting. Painting will automatically go to this instance and you don't
  /// have to manually bind any device context (this is all handled by the
  /// paint manager).
  /// @param client A non-owning pointer and must remain valid (normally the
  /// object implementing the Client interface will own the paint manager).
  /// @param is_always_opaque A flag passed to the device contexts that this
  /// class creates. Set this to true if your instance always draws an opaque
  /// image to the device. This is used as a hint to the browser that it does
  /// not need to do alpha blending, which speeds up painting. If you generate
  /// non-opqaue pixels or aren't sure, set this to false for more general
  /// blending.
  ///
  /// If you set <code>is_always_opaque</code>, your alpha channel should
  /// always be set to <code>0xFF</code> or there may be painting artifacts.
  /// Being opaque will allow the browser to do a memcpy rather than a blend
  /// to paint the plugin, and this means your alpha values will get set on the
  /// page backing store. If these values are incorrect, it could mess up
  /// future blending. If you aren't sure, it is always correct to specify that
  /// it it not opaque.
  void Initialize(Instance* instance, Client* client, bool is_always_opaque);

  /// Setter function setting the max ratio of paint rect area to scroll rect
  /// area that we will tolerate before downgrading the scroll into a repaint.
  ///
  /// If the combined area of paint rects contained within the scroll
  /// rect grows too large, then we might as well just treat
  /// the scroll rect as a paint rect.
  ///
  /// @param[in] area The max ratio of paint rect area to scroll rect area that
  /// we will tolerate before downgrading the scroll into a repaint.
  void set_max_redundant_paint_to_scroll_area(float area) {
    aggregator_.set_max_redundant_paint_to_scroll_area(area);
  }

  /// Setter function for setting the maximum number of paint rects. If we
  /// exceed this limit, then we'll start combining paint rects (refer to
  /// CombinePaintRects() for further information). This limiting can be
  /// important since there is typically some overhead in deciding what to
  /// paint. If your module is fast at doing these computations, raise this
  /// threshold, if your module is slow, lower it (probably requires some
  /// tuning to find the right value).
  ///
  /// @param[in] max_rects The maximum number of paint rects.
  void set_max_paint_rects(size_t max_rects) {
    aggregator_.set_max_paint_rects(max_rects);
  }

  /// SetSize() sets the size of the instance. If the size is the same as the
  /// previous call, this will be a NOP. If the size has changed, a new device
  /// will be allocated to the given size and a paint to that device will be
  /// scheduled.
  ///
  /// This function is intended to be called from <code>ViewChanged</code> with
  /// the size of the instance. Since it tracks the old size and only allocates
  /// when the size changes, you can always call this function without worrying
  /// about whether the size changed or ViewChanged() is called for another
  /// reason (like the position changed).
  ///
  /// @param new_size The new size for the instance.
  void SetSize(const Size& new_size);

  /// This function provides access to the underlying device in case you need
  /// it. If you have done a SetSize(), note that the graphics context won't be
  /// updated until right before the next call to OnPaint().
  ///
  /// <strong>Note:</strong> If you call Flush on this device the paint manager
  /// will get very confused, don't do this!
  const Graphics2D& graphics() const { return graphics_; }

  /// This function provides access to the underlying device in case you need
  /// it. If you have done a SetSize(), note that the graphics context won't be
  /// updated until right before the next call to OnPaint().
  ///
  /// <strong>Note:</strong> If you call Flush on this device the paint manager
  /// will get very confused, don't do this!
  Graphics2D& graphics() { return graphics_; }

  /// Invalidate() invalidate the entire instance.
  void Invalidate();

  /// InvalidateRect() Invalidate the provided rect.
  ///
  /// @param[in] rect The <code>Rect</code> to be invalidated.
  void InvalidateRect(const Rect& rect);

  /// ScrollRect() scrolls the provided <code>clip_rect</code> by the
  /// <code>amount</code> argument.
  ///
  /// @param clip_rect The clip rectangle to scroll.
  /// @param amount The amount to scroll <code>clip_rect</code>.
  void ScrollRect(const Rect& clip_rect, const Point& amount);

  /// GetEffectiveSize() returns the size of the graphics context for the
  /// next paint operation. This is the pending size if a resize is pending
  /// (the instance has called SetSize() but we haven't actually painted it
  /// yet), or the current size of no resize is pending.
  ///
  /// @return The effective size.
  Size GetEffectiveSize() const;

 private:
  // Disallow copy and assign (these are unimplemented).
  PaintManager(const PaintManager&);
  PaintManager& operator=(const PaintManager&);

  // Makes sure there is a callback that will trigger a paint at a later time.
  // This will be either a Flush callback telling us we're allowed to generate
  // more data, or, if there's no flush callback pending, a manual call back
  // to the message loop via ExecuteOnMainThread.
  void EnsureCallbackPending();

  // Does the client paint and executes a Flush if necessary.
  void DoPaint();

  // Callback for asynchronous completion of Flush.
  void OnFlushComplete(int32_t result);

  // Callback for manual scheduling of paints when there is no flush callback
  // pending.
  void OnManualCallbackComplete(int32_t);

  Instance* instance_;

  // Non-owning pointer. See the constructor.
  Client* client_;

  bool is_always_opaque_;

  CompletionCallbackFactory<PaintManager> callback_factory_;

  // This graphics device will be is_null() if no graphics has been manually
  // set yet.
  Graphics2D graphics_;

  PaintAggregator aggregator_;

  // See comment for EnsureCallbackPending for more on how these work.
  bool manual_callback_pending_;
  bool flush_pending_;

  // When we get a resize, we don't bind right away (see SetSize). The
  // has_pending_resize_ tells us that we need to do a resize for the next
  // paint operation. When true, the new size is in pending_size_.
  bool has_pending_resize_;
  Size pending_size_;
};

}  // namespace pp

#endif  // PPAPI_UTILITY_GRAPHICS_PAINT_MANAGER_H_
