/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_graphics_2d.idl modified Wed Apr 20 13:37:06 2016. */

#ifndef PPAPI_C_PPB_GRAPHICS_2D_H_
#define PPAPI_C_PPB_GRAPHICS_2D_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_GRAPHICS_2D_INTERFACE_1_0 "PPB_Graphics2D;1.0"
#define PPB_GRAPHICS_2D_INTERFACE_1_1 "PPB_Graphics2D;1.1"
#define PPB_GRAPHICS_2D_INTERFACE_1_2 "PPB_Graphics2D;1.2"
#define PPB_GRAPHICS_2D_INTERFACE PPB_GRAPHICS_2D_INTERFACE_1_2

/**
 * @file
 * Defines the <code>PPB_Graphics2D</code> struct representing a 2D graphics
 * context within the browser.
 */


/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * <code>PPB_Graphics2D</code> defines the interface for a 2D graphics context.
 */
struct PPB_Graphics2D_1_2 {
  /**
   * Create() creates a 2D graphics context. The returned graphics context will
   * not be bound to the module instance on creation (call BindGraphics() on
   * the module instance to bind the returned graphics context to the module
   * instance).
   *
   * @param[in] instance The module instance.
   * @param[in] size The size of the graphic context.
   * @param[in] is_always_opaque Set the <code>is_always_opaque</code> flag to
   * <code>PP_TRUE</code> if you know that you will be painting only opaque
   * data to this context. This option will disable blending when compositing
   * the module with the web page, which might give higher performance on some
   * computers.
   *
   * If you set <code>is_always_opaque</code>, your alpha channel should always
   * be set to 0xFF or there may be painting artifacts. The alpha values
   * overwrite the destination alpha values without blending when
   * <code>is_always_opaque</code> is true.
   *
   * @return A <code>PP_Resource</code> containing the 2D graphics context if
   * successful or 0 if unsuccessful.
   */
  PP_Resource (*Create)(PP_Instance instance,
                        const struct PP_Size* size,
                        PP_Bool is_always_opaque);
  /**
   * IsGraphics2D() determines if the given resource is a valid
   * <code>Graphics2D</code>.
   *
   * @param[in] resource A <code>Graphics2D</code> context resource.
   *
   * @return PP_TRUE if the given resource is a valid <code>Graphics2D</code>,
   * <code>PP_FALSE</code> if it is an invalid resource or is a resource of
   * another type.
   */
  PP_Bool (*IsGraphics2D)(PP_Resource resource);
  /**
   * Describe() retrieves the configuration for the given graphics context,
   * filling the given values (which must not be <code>NULL</code>).
   *
   * @param[in] resource The 2D Graphics resource.
   * @param[in,out] size The size of the 2D graphics context in the browser.
   * @param[in,out] is_always_opaque Identifies whether only opaque data
   * will be painted.
   *
   * @return Returns <code>PP_TRUE</code> on success or <code>PP_FALSE</code> if
   * the resource is invalid. The output parameters will be set to 0 on a
   * <code>PP_FALSE</code>.
   */
  PP_Bool (*Describe)(PP_Resource graphics_2d,
                      struct PP_Size* size,
                      PP_Bool* is_always_opaque);
  /**
   * PaintImageData() enqueues a paint of the given image into the context.
   * This function has no effect until you call Flush() As a result, what
   * counts is the contents of the bitmap when you call Flush(), not when
   * you call this function.
   *
   * The provided image will be placed at <code>top_left</code> from the top
   *  left of the context's internal backing store. Then the pixels contained
   * in <code>src_rect</code> will be copied into the backing store. This
   * means that the rectangle being painted will be at <code>src_rect</code>
   * offset by <code>top_left</code>.
   *
   * The <code>src_rect</code> is specified in the coordinate system of the
   * image being painted, not the context. For the common case of copying the
   * entire image, you may specify an empty <code>src_rect</code>.
   *
   * The painted area of the source bitmap must fall entirely within the
   * context. Attempting to paint outside of the context will result in an
   * error. However, the source bitmap may fall outside the context, as long
   * as the <code>src_rect</code> subset of it falls entirely within the
   * context.
   *
   * There are two methods most modules will use for painting. The first
   * method is to generate a new <code>ImageData</code> and then paint it. In
   * this case, you'll set the location of your painting to
   * <code>top_left</code> and set <code>src_rect</code> to <code>NULL</code>.
   * The second is that you're generating small invalid regions out of a larger
   * bitmap representing your entire instance. In this case, you would set the
   * location of your image to (0,0) and then set <code>src_rect</code> to the
   * pixels you changed.
   *
   * @param[in] resource The 2D Graphics resource.
   * @param[in] image The <code>ImageData</code> to be painted.
   * @param[in] top_left A <code>Point</code> representing the
   * <code>top_left</code> location where the <code>ImageData</code> will be
   * painted.
   * @param[in] src_rect The rectangular area where the <code>ImageData</code>
   * will be painted.
   */
  void (*PaintImageData)(PP_Resource graphics_2d,
                         PP_Resource image_data,
                         const struct PP_Point* top_left,
                         const struct PP_Rect* src_rect);
  /**
   * Scroll() enqueues a scroll of the context's backing store. This
   * function has no effect until you call Flush(). The data within the
   * provided clipping rectangle will be shifted by (dx, dy) pixels.
   *
   * This function will result in some exposed region which will have undefined
   * contents. The module should call PaintImageData() on these exposed regions
   * to give the correct contents.
   *
   * The scroll can be larger than the area of the clipping rectangle, which
   * means the current image will be scrolled out of the rectangle. This
   * scenario is not an error but will result in a no-op.
   *
   * @param[in] graphics_2d The 2D Graphics resource.
   * @param[in] clip The clipping rectangle.
   * @param[in] amount The amount the area in the clipping rectangle will
   * shifted.
   */
  void (*Scroll)(PP_Resource graphics_2d,
                 const struct PP_Rect* clip_rect,
                 const struct PP_Point* amount);
  /**
   * ReplaceContents() provides a slightly more efficient way to paint the
   * entire module's image. Normally, calling PaintImageData() requires that
   * the browser copy the pixels out of the image and into the graphics
   * context's backing store. This function replaces the graphics context's
   * backing store with the given image, avoiding the copy.
   *
   * The new image must be the exact same size as this graphics context. If the
   * new image uses a different image format than the browser's native bitmap
   * format (use <code>PPB_ImageData.GetNativeImageDataFormat()</code> to
   * retrieve the format), then a conversion will be done inside the browser
   * which may slow the performance a little bit.
   *
   * <strong>Note:</strong> The new image will not be painted until you call
   * Flush().
   *
   * After this call, you should take care to release your references to the
   * image. If you paint to the image after ReplaceContents(), there is the
   * possibility of significant painting artifacts because the page might use
   * partially-rendered data when copying out of the backing store.
   *
   * In the case of an animation, you will want to allocate a new image for the
   * next frame. It is best if you wait until the flush callback has executed
   * before allocating this bitmap. This gives the browser the option of
   * caching the previous backing store and handing it back to you (assuming
   * the sizes match). In the optimal case, this means no bitmaps are allocated
   * during the animation, and the backing store and "front buffer" (which the
   * plugin is painting into) are just being swapped back and forth.
   *
   * @param[in] graphics_2d The 2D Graphics resource.
   * @param[in] image The <code>ImageData</code> to be painted.
   */
  void (*ReplaceContents)(PP_Resource graphics_2d, PP_Resource image_data);
  /**
   * Flush() flushes any enqueued paint, scroll, and replace commands to the
   * backing store. This function actually executes the updates, and causes a
   * repaint of the webpage, assuming this graphics context is bound to a module
   * instance.
   *
   * Flush() runs in asynchronous mode. Specify a callback function and the
   * argument for that callback function. The callback function will be
   * executed on the calling thread when the image has been painted to the
   * screen. While you are waiting for a flush callback, additional calls to
   * Flush() will fail.
   *
   * Because the callback is executed (or thread unblocked) only when the
   * instance's image is actually on the screen, this function provides
   * a way to rate limit animations. By waiting until the image is on the
   * screen before painting the next frame, you can ensure you're not
   * flushing 2D graphics faster than the screen can be updated.
   *
   * <strong>Unbound contexts</strong>
   * If the context is not bound to a module instance, you will
   * still get a callback. The callback will execute after Flush() returns
   * to avoid reentrancy. The callback will not wait until anything is
   * painted to the screen because there will be nothing on the screen. The
   * timing of this callback is not guaranteed and may be deprioritized by
   * the browser because it is not affecting the user experience.
   *
   * <strong>Off-screen instances</strong>
   * If the context is bound to an instance that is currently not visible (for
   * example, scrolled out of view) it will behave like the "unbound context"
   * case.
   *
   * <strong>Detaching a context</strong>
   * If you detach a context from a module instance, any pending flush
   * callbacks will be converted into the "unbound context" case.
   *
   * <strong>Released contexts</strong>
   * A callback may or may not get called even if you have released all
   * of your references to the context. This scenario can occur if there are
   * internal references to the context suggesting it has not been internally
   * destroyed (for example, if it is still bound to an instance) or due to
   * other implementation details. As a result, you should be careful to
   * check that flush callbacks are for the context you expect and that
   * you're capable of handling callbacks for unreferenced contexts.
   *
   * <strong>Shutdown</strong>
   * If a module instance is removed when a flush is pending, the
   * callback will not be executed.
   *
   * @param[in] graphics_2d The 2D Graphics resource.
   * @param[in] callback A <code>CompletionCallback</code> to be called when
   * the image has been painted on the screen.
   *
   * @return Returns <code>PP_OK</code> on success or
   * <code>PP_ERROR_BADRESOURCE</code> if the graphics context is invalid,
   * <code>PP_ERROR_BADARGUMENT</code> if the callback is null and flush is
   * being called from the main thread of the module, or
   * <code>PP_ERROR_INPROGRESS</code> if a flush is already pending that has
   * not issued its callback yet.  In the failure case, nothing will be updated
   * and no callback will be scheduled.
   */
  int32_t (*Flush)(PP_Resource graphics_2d,
                   struct PP_CompletionCallback callback);
  /**
   * SetScale() sets the scale factor that will be applied when painting the
   * graphics context onto the output device. Typically, if rendering at device
   * resolution is desired, the context would be created with the width and
   * height scaled up by the view's GetDeviceScale and SetScale called with a
   * scale of 1.0 / GetDeviceScale(). For example, if the view resource passed
   * to DidChangeView has a rectangle of (w=200, h=100) and a device scale of
   * 2.0, one would call Create with a size of (w=400, h=200) and then call
   * SetScale with 0.5. One would then treat each pixel in the context as a
   * single device pixel.
   *
   * @param[in] resource A <code>Graphics2D</code> context resource.
   * @param[in] scale The scale to apply when painting.
   *
   * @return Returns <code>PP_TRUE</code> on success or <code>PP_FALSE</code> if
   * the resource is invalid or the scale factor is 0 or less.
   */
  PP_Bool (*SetScale)(PP_Resource resource, float scale);
  /***
   * GetScale() gets the scale factor that will be applied when painting the
   * graphics context onto the output device.
   *
   * @param[in] resource A <code>Graphics2D</code> context resource.
   *
   * @return Returns the scale factor for the graphics context. If the resource
   * is not a valid <code>Graphics2D</code> context, this will return 0.0.
   */
  float (*GetScale)(PP_Resource resource);
  /**
   * SetLayerTransform() sets a transformation factor that will be applied for
   * the current graphics context displayed on the output device.  If both
   * SetScale and SetLayerTransform will be used, they are going to get combined
   * for the final result.
   *
   * This function has no effect until you call Flush().
   *
   * @param[in] scale The scale to be applied.
   * @param[in] origin The origin of the scale.
   * @param[in] translate The translation to be applied.
   *
   * @return Returns <code>PP_TRUE</code> on success or <code>PP_FALSE</code>
   * if the resource is invalid or the scale factor is 0 or less.
   */
  PP_Bool (*SetLayerTransform)(PP_Resource resource,
                               float scale,
                               const struct PP_Point* origin,
                               const struct PP_Point* translate);
};

typedef struct PPB_Graphics2D_1_2 PPB_Graphics2D;

struct PPB_Graphics2D_1_0 {
  PP_Resource (*Create)(PP_Instance instance,
                        const struct PP_Size* size,
                        PP_Bool is_always_opaque);
  PP_Bool (*IsGraphics2D)(PP_Resource resource);
  PP_Bool (*Describe)(PP_Resource graphics_2d,
                      struct PP_Size* size,
                      PP_Bool* is_always_opaque);
  void (*PaintImageData)(PP_Resource graphics_2d,
                         PP_Resource image_data,
                         const struct PP_Point* top_left,
                         const struct PP_Rect* src_rect);
  void (*Scroll)(PP_Resource graphics_2d,
                 const struct PP_Rect* clip_rect,
                 const struct PP_Point* amount);
  void (*ReplaceContents)(PP_Resource graphics_2d, PP_Resource image_data);
  int32_t (*Flush)(PP_Resource graphics_2d,
                   struct PP_CompletionCallback callback);
};

struct PPB_Graphics2D_1_1 {
  PP_Resource (*Create)(PP_Instance instance,
                        const struct PP_Size* size,
                        PP_Bool is_always_opaque);
  PP_Bool (*IsGraphics2D)(PP_Resource resource);
  PP_Bool (*Describe)(PP_Resource graphics_2d,
                      struct PP_Size* size,
                      PP_Bool* is_always_opaque);
  void (*PaintImageData)(PP_Resource graphics_2d,
                         PP_Resource image_data,
                         const struct PP_Point* top_left,
                         const struct PP_Rect* src_rect);
  void (*Scroll)(PP_Resource graphics_2d,
                 const struct PP_Rect* clip_rect,
                 const struct PP_Point* amount);
  void (*ReplaceContents)(PP_Resource graphics_2d, PP_Resource image_data);
  int32_t (*Flush)(PP_Resource graphics_2d,
                   struct PP_CompletionCallback callback);
  PP_Bool (*SetScale)(PP_Resource resource, float scale);
  float (*GetScale)(PP_Resource resource);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_GRAPHICS_2D_H_ */

