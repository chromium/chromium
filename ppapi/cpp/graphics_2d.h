// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_GRAPHICS_2D_H_
#define PPAPI_CPP_GRAPHICS_2D_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/size.h"


/// @file
/// This file defines the API to create a 2D graphics context in the browser.
namespace pp {

class CompletionCallback;
class ImageData;
class InstanceHandle;
class Point;
class Rect;

class Graphics2D : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>Graphics2D</code>
  /// object.
  Graphics2D();

  /// The copy constructor for Graphics2D. The underlying 2D context is not
  /// copied; this constructor creates another reference to the original 2D
  /// context.
  ///
  /// @param[in] other A pointer to a <code>Graphics2D</code> context.
  Graphics2D(const Graphics2D& other);

  /// A constructor allocating a new 2D graphics context with the given size
  /// in the browser, resulting object will be is_null() if the allocation
  /// failed.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  ///
  /// @param[in] size The size of the 2D graphics context in the browser,
  /// measured in pixels. See <code>SetScale()</code> for more information.
  ///
  /// @param[in] is_always_opaque Set the <code>is_always_opaque</code> flag
  /// to true if you know that you will be painting only opaque data to this
  /// context. This option will disable blending when compositing the module
  /// with the web page, which might give higher performance on some computers.
  ///
  /// If you set <code>is_always_opaque</code>, your alpha channel should
  /// always be set to 0xFF or there may be painting artifacts. The alpha values
  /// overwrite the destination alpha values without blending when
  /// <code>is_always_opaque</code> is true.
  Graphics2D(const InstanceHandle& instance,
             const Size& size,
             bool is_always_opaque);

  /// A destructor that decrements the reference count of a
  /// <code>Graphics2D</code> object made using the previous copy constructor.
  /// It is possible that the destructor does not totally destroy the underlying
  /// 2D context if there are outstanding references to it.
  virtual ~Graphics2D();

  /// This function assigns one 2D graphics context to this 2D graphics
  /// context. This function increases the reference count of the 2D resource
  /// of the other 2D graphics context while decrementing the reference counter
  /// of this 2D graphics context.
  ///
  /// @param[in] other An other 2D graphics context.
  ///
  /// @return A new Graphics2D context.
  Graphics2D& operator=(const Graphics2D& other);

  /// Getter function for returning size of the 2D graphics context.
  ///
  /// @return The size of the 2D graphics context measured in pixels.
  const Size& size() const { return size_; }

  /// PaintImageData() enqueues a paint command of the given image into
  /// the context. This command has no effect until you call Flush(). As a
  /// result, what counts is the contents of the bitmap when you call Flush,
  /// not when you call this function.
  ///
  /// The provided image will be placed at <code>top_left</code> from the top
  /// left of the context's internal backing store. This version of
  /// PaintImageData paints the entire image. Refer to the other version of
  /// this function to paint only part of the area.
  ///
  /// The painted area of the source bitmap must fall entirely within the
  /// context. Attempting to paint outside of the context will result in an
  /// error.
  ///
  /// There are two methods most modules will use for painting. The first
  /// method is to generate a new <code>ImageData</code> and then paint it.
  /// In this case, you'll set the location of your painting to
  /// <code>top_left</code> and set <code>src_rect</code> to <code>NULL</code>.
  /// The second is that you're generating small invalid regions out of a larger
  /// bitmap representing your entire module's image.
  ///
  /// @param[in] image The <code>ImageData</code> to be painted.
  /// @param[in] top_left A <code>Point</code> representing the
  /// <code>top_left</code> location where the <code>ImageData</code> will be
  /// painted.
  void PaintImageData(const ImageData& image,
                      const Point& top_left);

  /// PaintImageData() enqueues a paint command of the given image into
  /// the context. This command has no effect until you call Flush(). As a
  /// result, what counts is the contents of the bitmap when you call Flush(),
  /// not when you call this function.
  ///
  /// The provided image will be placed at <code>top_left</code> from the top
  /// left of the context's internal backing store. Then the pixels contained
  /// in <code>src_rect</code> will be copied into the backing store. This
  /// means that the rectangle being painted will be at <code>src_rect</code>
  /// offset by <code>top_left</code>.
  ///
  /// The <code>src_rect</code> is specified in the coordinate system of the
  /// image being painted, not the context. For the common case of copying the
  /// entire image, you may specify an empty <code>src_rect</code>.
  ///
  /// The painted area of the source bitmap must fall entirely within the
  /// context. Attempting to paint outside of the context will result in an
  /// error. However, the source bitmap may fall outside the context, as long
  /// as the <code>src_rect</code> subset of it falls entirely within the
  /// context.
  ///
  /// There are two methods most modules will use for painting. The first
  /// method is to generate a new <code>ImageData</code> and then paint it. In
  /// this case, you'll set the location of your painting to
  /// <code>top_left</code> and set <code>src_rect</code> to <code>NULL</code>.
  /// The second is that you're generating small invalid regions out of a larger
  /// bitmap representing your entire module. In this case, you would set the
  /// location of your image to (0,0) and then set <code>src_rect</code> to the
  /// pixels you changed.
  ///
  /// @param[in] image The <code>ImageData</code> to be painted.
  /// @param[in] top_left A <code>Point</code> representing the
  /// <code>top_left</code> location where the <code>ImageData</code> will be
  /// painted.
  /// @param[in] src_rect The rectangular area where the <code>ImageData</code>
  /// will be painted.
  void PaintImageData(const ImageData& image,
                      const Point& top_left,
                      const Rect& src_rect);

  /// Scroll() enqueues a scroll of the context's backing store. This
  /// function has no effect until you call Flush(). The data within the
  /// provided clipping rectangle will be shifted by (dx, dy) pixels.
  ///
  /// This function will result in some exposed region which will have
  /// undefined contents. The module should call PaintImageData() on
  /// these exposed regions to give the correct contents.
  ///
  /// The scroll can be larger than the area of the clipping rectangle, which
  /// means the current image will be scrolled out of the rectangle. This
  /// scenario is not an error but will result in a no-op.
  ///
  /// @param[in] clip The clipping rectangle.
  /// @param[in] amount The amount the area in the clipping rectangle will
  /// shifted.
  void Scroll(const Rect& clip, const Point& amount);

  /// ReplaceContents() provides a slightly more efficient way to paint the
  /// entire module's image. Normally, calling PaintImageData() requires that
  /// the browser copy the pixels out of the image and into the graphics
  /// context's backing store. This function replaces the graphics context's
  /// backing store with the given image, avoiding the copy.
  ///
  /// The new image must be the exact same size as this graphics context. If
  /// the new image uses a different image format than the browser's native
  /// bitmap format (use ImageData::GetNativeImageDataFormat() to retrieve the
  /// format), then a conversion will be done inside the browser which may slow
  /// the performance a little bit.
  ///
  /// <strong>Note:</strong> The new image will not be painted until you call
  /// Flush().
  ///
  /// After this call, you should take care to release your references to the
  /// image. If you paint to the image after ReplaceContents(), there is the
  /// possibility of significant painting artifacts because the page might use
  /// partially-rendered data when copying out of the backing store.
  ///
  /// In the case of an animation, you will want to allocate a new image for
  /// the next frame. It is best if you wait until the flush callback has
  /// executed before allocating this bitmap. This gives the browser the option
  /// of caching the previous backing store and handing it back to you
  /// (assuming the sizes match). In the optimal case, this means no bitmaps are
  /// allocated during the animation, and the backing store and "front buffer"
  /// (which the module is painting into) are just being swapped back and forth.
  ///
  /// @param[in] image The <code>ImageData</code> to be painted.
  void ReplaceContents(ImageData* image);

  /// Flush() flushes any enqueued paint, scroll, and replace commands
  /// to the backing store. This actually executes the updates, and causes a
  /// repaint of the webpage, assuming this graphics context is bound to a
  /// module instance.
  ///
  /// Flush() runs in asynchronous mode. Specify a callback function and
  /// the argument for that callback function. The callback function will be
  /// executed on the calling thread when the image has been painted to the
  /// screen. While you are waiting for a <code>Flush</code> callback,
  /// additional calls to Flush() will fail.
  ///
  /// Because the callback is executed (or thread unblocked) only when the
  /// module's image is actually on the screen, this function provides
  /// a way to rate limit animations. By waiting until the image is on the
  /// screen before painting the next frame, you can ensure you're not
  /// flushing 2D graphics faster than the screen can be updated.
  ///
  /// <strong>Unbound contexts</strong>
  /// If the context is not bound to a module instance, you will
  /// still get a callback. The callback will execute after Flush() returns
  /// to avoid reentrancy. The callback will not wait until anything is
  /// painted to the screen because there will be nothing on the screen. The
  /// timing of this callback is not guaranteed and may be deprioritized by
  /// the browser because it is not affecting the user experience.
  ///
  /// <strong>Off-screen instances</strong>
  /// If the context is bound to an instance that is
  /// currently not visible (for example, scrolled out of view) it will
  /// behave like the "unbound context" case.
  ///
  /// <strong>Detaching a context</strong>
  /// If you detach a context from a module instance, any
  /// pending flush callbacks will be converted into the "unbound context"
  /// case.
  ///
  /// <strong>Released contexts</strong>
  /// A callback may or may not still get called even if you have released all
  /// of your references to the context. This can occur if there are internal
  /// references to the context that means it has not been internally
  /// destroyed (for example, if it is still bound to an instance) or due to
  /// other implementation details. As a result, you should be careful to
  /// check that flush callbacks are for the context you expect and that
  /// you're capable of handling callbacks for context that you may have
  /// released your reference to.
  ///
  /// <strong>Shutdown</strong>
  /// If a module instance is removed when a Flush is pending, the
  /// callback will not be executed.
  ///
  /// @param[in] cc A <code>CompletionCallback</code> to be called when the
  /// image has been painted on the screen.
  ///
  /// @return Returns <code>PP_OK</code> on success or
  /// <code>PP_ERROR_BADRESOURCE</code> if the graphics context is invalid,
  /// <code>PP_ERROR_BADARGUMENT</code> if the callback is null and
  /// flush is being called from the main thread of the module, or
  /// <code>PP_ERROR_INPROGRESS</code> if a flush is already pending that has
  /// not issued its callback yet.  In the failure case, nothing will be
  /// updated and no callback will be scheduled.

  // TODO(darin): We should ensure that the completion callback always runs, so
  // that it is easier for consumers to manage memory referenced by a callback.

  // TODO(crbug.com/): Add back in the synchronous mode description once we have
  // support for it.
  int32_t Flush(const CompletionCallback& cc);

  /// SetScale() sets the scale factor that will be applied when painting the
  /// graphics context onto the output device. Typically, if rendering at device
  /// resolution is desired, the context would be created with the width and
  /// height scaled up by the view's GetDeviceScale and SetScale called with a
  /// scale of 1.0 / GetDeviceScale(). For example, if the view resource passed
  /// to DidChangeView has a rectangle of (w=200, h=100) and a device scale of
  /// 2.0, one would call Create with a size of (w=400, h=200) and then call
  /// SetScale with 0.5. One would then treat each pixel in the context as a
  /// single device pixel.
  ///
  /// @param[in] scale The scale to apply when painting.
  ///
  /// @return Returns <code>true</code> on success or <code>false</code>
  /// if the resource is invalid or the scale factor is 0 or less.
  bool SetScale(float scale);

  /// GetScale() gets the scale factor that will be applied when painting the
  /// graphics context onto the output device.
  ///
  /// @return Returns the scale factor for the graphics context. If the resource
  /// is invalid, 0.0 will be returned. The default scale for a graphics context
  /// is 1.0.
  float GetScale();

  bool SetLayerTransform(float scale,
                         const Point& origin,
                         const Point& translate);
 private:
  Size size_;
};

}  // namespace pp

#endif  // PPAPI_CPP_GRAPHICS_2D_H_
