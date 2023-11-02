/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <stdlib.h>
#include <string.h>

#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_view.h"

#include "ppapi_simple/ps.h"
#include "ppapi_simple/ps_context_2d.h"
#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps_instance.h"
#include "ppapi_simple/ps_interface.h"

PSContext2D_t* PSContext2DAllocate(PP_ImageDataFormat format) {
  PSContext2D_t* ctx = (PSContext2D_t*)malloc(sizeof(PSContext2D_t));
  memset(ctx, 0, sizeof(PSContext2D_t));

  ctx->format = format;
  return ctx;
}

void PSContext2DFree(PSContext2D_t* ctx) {
  if (ctx->graphic_2d) {
    PSInterfaceCore()->ReleaseResource(ctx->graphic_2d);
    ctx->graphic_2d = 0;
  }
  if (ctx->image) {
    PSInterfaceCore()->ReleaseResource(ctx->image);
    ctx->image = 0;
  }
  free(ctx);
}

PP_ImageDataFormat PSContext2DGetNativeImageDataFormat() {
  return PSInterfaceImageData()->GetNativeImageDataFormat();
}

// Update the 2D context if the message is appropriate, returning non-zero
// if the event was consumed.
int PSContext2DHandleEvent(PSContext2D_t* ctx, PSEvent* event) {
  switch (event->type) {
    case PSE_INSTANCE_DIDCHANGEVIEW: {
      struct PP_Rect rect;

      PSInterfaceView()->GetRect(event->as_resource, &rect);
      PSInterfaceCore()->ReleaseResource(ctx->graphic_2d);
      ctx->bound = 0;
      ctx->width = rect.size.width;
      ctx->height = rect.size.height;

      // Create an opaque graphic context of the specified size.
      ctx->graphic_2d = PSInterfaceGraphics2D()->Create(PSGetInstanceId(),
                                                        &rect.size, PP_TRUE);

      // Bind the context to so that draws will be visible.
      if (ctx->graphic_2d) {
        ctx->bound = PSInterfaceInstance()->BindGraphics(PSGetInstanceId(),
                                                         ctx->graphic_2d);
      }

      // Typically this resource would not be allocated yet, but just in case
      // throw it away, to force a new allocation when GetBuffer is called.
      if (ctx->image) {
        PSInterfaceCore()->ReleaseResource(ctx->image);
        ctx->image = 0;
      }

      return 1;
    }
    default:
      break;
  }

  return 0;
}

// Allocates (if needed) a new image context which will be swapped in when
// drawing is complete.  PSContextGetBuffer and PSContext2DSwapBuffer
// implemented the suggested image/graphic_2d use specified in the
// ppb_graphics_2d header.
int PSContext2DGetBuffer(PSContext2D_t* ctx) {
  if (!ctx->bound)
    return 0;

  // Check if we are already holding an image
  if (ctx->image)
    return 1;

  struct PP_Size size;
  size.width = ctx->width;
  size.height = ctx->height;

  // Allocate a new image resource with the specified size and format, but
  // do not ZERO out the buffer first since we will fill it.
  PP_Resource image = PSInterfaceImageData()->Create(
      PSGetInstanceId(), ctx->format, &size, PP_FALSE);

  if (0 == image) {
    PSInstanceError("Unable to create 2D image.\n");
    return 0;
  }

  // Get the stride
  struct PP_ImageDataDesc desc;
  PSInterfaceImageData()->Describe(image, &desc);

  ctx->image = image;
  ctx->data = (uint32_t*)(PSInterfaceImageData()->Map(image));
  ctx->stride = desc.stride;
  return 1;
}

int PSContext2DSwapBuffer(PSContext2D_t* ctx) {
  if (ctx->bound && ctx->image) {
    PSInterfaceImageData()->Unmap(ctx->image);
    PSInterfaceGraphics2D()->ReplaceContents(ctx->graphic_2d, ctx->image);
    PSInterfaceGraphics2D()->Flush(ctx->graphic_2d, PP_BlockUntilComplete());
    PSInterfaceCore()->ReleaseResource(ctx->image);

    ctx->image = 0;
    ctx->stride = 0;
    ctx->data = NULL;
    return 1;
  }
  return 0;
}
