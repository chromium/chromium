/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_PPAPI_SIMPLE_PS_CONTEXT_2D_H_
#define LIBRARIES_PPAPI_SIMPLE_PS_CONTEXT_2D_H_

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"

#include "ppapi_simple/ps_event.h"

#include "sdk_util/macros.h"

EXTERN_C_BEGIN

typedef struct {
  int bound;
  int32_t width;
  int32_t height;
  uint32_t stride;
  uint32_t* data;
  PP_ImageDataFormat format;
  PP_Resource graphic_2d;
  PP_Resource image;
} PSContext2D_t;


/*
 * PSContext2DAllocate
 *
 * Allocate or free a 2D context object which the library can use to perform
 * various PPAPI operations on the developer's behalf, such as processing view
 * change events, swapping buffers, etc...
 */
PSContext2D_t* PSContext2DAllocate(PP_ImageDataFormat format);
void PSContext2DFree(PSContext2D_t* ctx);

/*
 * PSContext2DGetNativeFormat
 *
 * Query the native system image format.
 */
PP_ImageDataFormat PSContext2DGetNativeImageDataFormat();

/*
 * PSContext2DHandleEvent
 *
 * Updates the context such as allocating, freeing, or sizing graphics and
 * image resources in response to events.
 */
int PSContext2DHandleEvent(PSContext2D_t* ctx, PSEvent* event);

/*
 * PSContext2DGetBuffer
 *
 * Points the data member of the context to the raw pixels of the image for
 * writing to the screen.  The image will become visible after a swap.
 */
int PSContext2DGetBuffer(PSContext2D_t* ctx);

/*
 * PSContext2DSwapBuffer
 *
 * Swaps out the currently visible graphics with the data stored in the image
 * buffer making it visible.  The old image resource will no longer be
 * available after this call.
 */
int PSContext2DSwapBuffer(PSContext2D_t* ctx);

EXTERN_C_END

#endif  // LIBRARIES_PPAPI_SIMPLE_PS_CONTEXT_2D_H_
