/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_view.h"

#include "ppapi_simple/ps_event.h"
#include "ppapi_simple/ps.h"

PPB_Core* g_pCore;
PPB_Fullscreen* g_pFullscreen;
PPB_Graphics2D* g_pGraphics2D;
PPB_ImageData* g_pImageData;
PPB_Instance* g_pInstance;
PPB_View* g_pView;
PPB_InputEvent* g_pInputEvent;
PPB_KeyboardInputEvent* g_pKeyboardInput;
PPB_MouseInputEvent* g_pMouseInput;
PPB_TouchInputEvent* g_pTouchInput;

struct {
  PP_Resource ctx;
  struct PP_Size size;
  int bound;
  uint8_t* cell_in;
  uint8_t* cell_out;
} g_Context;


const unsigned int kInitialRandSeed = 0xC0DE533D;

/* BGRA helper macro, for constructing a pixel for a BGRA buffer. */
#define MakeBGRA(b, g, r, a)  \
  (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))


/*
 * Convert a count value into a live (green) or dead color value.
 */
const uint32_t kNeighborColors[] = {
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0xFF, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
    MakeBGRA(0x00, 0x00, 0x00, 0xFF),
};

/*
 * These represent the new health value of a cell based on its neighboring
 * values.  The health is binary: either alive or dead.
 */
const uint8_t kIsAlive[] = {
      0, 0, 0, 0, 0, 1, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0
};

void UpdateContext(uint32_t width, uint32_t height) {
  if (width != g_Context.size.width || height != g_Context.size.height) {
    size_t size = width * height;
    size_t index;

    free(g_Context.cell_in);
    free(g_Context.cell_out);

    /* Create a new context */
    g_Context.cell_in = (uint8_t*) malloc(size);
    g_Context.cell_out = (uint8_t*) malloc(size);

    memset(g_Context.cell_out, 0, size);
    for (index = 0; index < size; index++) {
      g_Context.cell_in[index] = rand() & 1;
    }
  }

  /* Recreate the graphics context on a view change */
  g_pCore->ReleaseResource(g_Context.ctx);
  g_Context.size.width = width;
  g_Context.size.height = height;
  g_Context.ctx =
      g_pGraphics2D->Create(PSGetInstanceId(), &g_Context.size, PP_TRUE);
  g_Context.bound =
      g_pInstance->BindGraphics(PSGetInstanceId(), g_Context.ctx);
}

void DrawCell(int32_t x, int32_t y) {
  int32_t width = g_Context.size.width;
  int32_t height = g_Context.size.height;

  if (!g_Context.cell_in) return;

  if (x > 0 && x < width - 1 && y > 0 && y < height - 1) {
    g_Context.cell_in[x - 1 + y * width] = 1;
    g_Context.cell_in[x + 1 + y * width] = 1;
    g_Context.cell_in[x + (y - 1) * width] = 1;
    g_Context.cell_in[x + (y + 1) * width] = 1;
  }
}

void ProcessTouchEvent(PSEvent* event) {
  uint32_t count = g_pTouchInput->GetTouchCount(event->as_resource,
      PP_TOUCHLIST_TYPE_TOUCHES);
  uint32_t i, j;
  for (i = 0; i < count; i++) {
    struct PP_TouchPoint touch = g_pTouchInput->GetTouchByIndex(
        event->as_resource, PP_TOUCHLIST_TYPE_TOUCHES, i);
    int radius = (int)touch.radius.x;
    int x = (int)touch.position.x;
    int y = (int)touch.position.y;
    /* num = 1/100th the area of touch point */
    int num = (int)(M_PI * radius * radius / 100.0f);
    for (j = 0; j < num; j++) {
      int dx = rand() % (radius * 2) - radius;
      int dy = rand() % (radius * 2) - radius;
      /* only plot random cells within the touch area */
      if (dx * dx + dy * dy <= radius * radius)
        DrawCell(x + dx, y + dy);
    }
  }
}

void ProcessEvent(PSEvent* event) {
  switch(event->type) {
    /* If the view updates, build a new Graphics 2D Context */
    case PSE_INSTANCE_DIDCHANGEVIEW: {
      struct PP_Rect rect;

      g_pView->GetRect(event->as_resource, &rect);
      UpdateContext(rect.size.width, rect.size.height);
      break;
    }

    case PSE_INSTANCE_HANDLEINPUT: {
      PP_InputEvent_Type type = g_pInputEvent->GetType(event->as_resource);
      PP_InputEvent_Modifier modifiers =
          g_pInputEvent->GetModifiers(event->as_resource);

      switch(type) {
        case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        case PP_INPUTEVENT_TYPE_MOUSEMOVE: {
          struct PP_Point location =
              g_pMouseInput->GetPosition(event->as_resource);
          /* If the button is down, draw */
          if (modifiers & PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN) {
            DrawCell(location.x, location.y);
          }
          break;
        }

        case PP_INPUTEVENT_TYPE_TOUCHSTART:
        case PP_INPUTEVENT_TYPE_TOUCHMOVE:
          ProcessTouchEvent(event);
          break;

        case PP_INPUTEVENT_TYPE_KEYDOWN: {
          PP_Bool fullscreen = g_pFullscreen->IsFullscreen(PSGetInstanceId());
          g_pFullscreen->SetFullscreen(PSGetInstanceId(),
                                       fullscreen ? PP_FALSE : PP_TRUE);
          break;
        }

        default:
          break;
      }
      /* case PSE_INSTANCE_HANDLEINPUT */
      break;
    }

    default:
      break;
  }
}


void Stir(uint32_t width, uint32_t height) {
  int i;
  if (g_Context.cell_in == NULL || g_Context.cell_out == NULL)
    return;

  for (i = 0; i < width; ++i) {
    g_Context.cell_in[i] = rand() & 1;
    g_Context.cell_in[i + (height - 1) * width] = rand() & 1;
  }
  for (i = 0; i < height; ++i) {
    g_Context.cell_in[i * width] = rand() & 1;
    g_Context.cell_in[i * width + (width - 1)] = rand() & 1;
  }
}

void Render() {
  struct PP_Size* psize = &g_Context.size;
  PP_ImageDataFormat format = PP_IMAGEDATAFORMAT_BGRA_PREMUL;

  /*
   * Create a buffer to draw into.  Since we are waiting until the next flush
   * chrome has an opportunity to cache this buffer see ppb_graphics_2d.h.
   */
  PP_Resource image =
      g_pImageData->Create(PSGetInstanceId(), format, psize, PP_FALSE);
  uint8_t* pixels = g_pImageData->Map(image);

  struct PP_ImageDataDesc desc;
  uint8_t* cell_temp;
  uint32_t x, y;

  /* If we somehow have not allocated these pointers yet, skip this frame. */
  if (!g_Context.cell_in || !g_Context.cell_out) return;

  /* Get the stride. */
  g_pImageData->Describe(image, &desc);

  /* Stir up the edges to prevent the simulation from reaching steady state. */
  Stir(desc.size.width, desc.size.height);

  /* Do neighbor summation; apply rules, output pixel color. */
  for (y = 1; y < desc.size.height - 1; ++y) {
    uint8_t *src0 = (g_Context.cell_in + (y - 1) * desc.size.width) + 1;
    uint8_t *src1 = src0 + desc.size.width;
    uint8_t *src2 = src1 + desc.size.width;
    int count;
    uint32_t color;
    uint8_t *dst = (g_Context.cell_out + y * desc.size.width) + 1;
    uint32_t *pixel_line =  (uint32_t*) (pixels + y * desc.stride);

    for (x = 1; x < (desc.size.width - 1); ++x) {
      /* Jitter and sum neighbors. */
      count = src0[-1] + src0[0] + src0[1] +
              src1[-1] +         + src1[1] +
              src2[-1] + src2[0] + src2[1];
      /* Include center cell. */
      count = count + count + src1[0];
      /* Use table lookup indexed by count to determine pixel & alive state. */
      color = kNeighborColors[count];
      *pixel_line++ = color;
      *dst++ = kIsAlive[count];
      ++src0;
      ++src1;
      ++src2;
    }
  }

  cell_temp = g_Context.cell_in;
  g_Context.cell_in = g_Context.cell_out;
  g_Context.cell_out = cell_temp;

  /* Unmap the range, we no longer need it. */
  g_pImageData->Unmap(image);

  /* Replace the contexts, and block until it's on the screen. */
  g_pGraphics2D->ReplaceContents(g_Context.ctx, image);
  g_pGraphics2D->Flush(g_Context.ctx, PP_BlockUntilComplete());

  /* Release the image data, we no longer need it. */
  g_pCore->ReleaseResource(image);
}

/*
 * Starting point for the module.  We do not use main since it would
 * collide with main in libppapi_cpp.
 */
int main(int argc, char *argv[]) {
  fprintf(stdout,"Started main.\n");
  g_pCore = (PPB_Core*)PSGetInterface(PPB_CORE_INTERFACE);
  g_pFullscreen = (PPB_Fullscreen*)PSGetInterface(PPB_FULLSCREEN_INTERFACE);
  g_pGraphics2D = (PPB_Graphics2D*)PSGetInterface(PPB_GRAPHICS_2D_INTERFACE);
  g_pInstance = (PPB_Instance*)PSGetInterface(PPB_INSTANCE_INTERFACE);
  g_pImageData = (PPB_ImageData*)PSGetInterface(PPB_IMAGEDATA_INTERFACE);
  g_pView = (PPB_View*)PSGetInterface(PPB_VIEW_INTERFACE);

  g_pInputEvent =
      (PPB_InputEvent*) PSGetInterface(PPB_INPUT_EVENT_INTERFACE);
  g_pKeyboardInput = (PPB_KeyboardInputEvent*)
      PSGetInterface(PPB_KEYBOARD_INPUT_EVENT_INTERFACE);
  g_pMouseInput =
      (PPB_MouseInputEvent*) PSGetInterface(PPB_MOUSE_INPUT_EVENT_INTERFACE);
  g_pTouchInput =
      (PPB_TouchInputEvent*) PSGetInterface(PPB_TOUCH_INPUT_EVENT_INTERFACE);

  PSEventSetFilter(PSE_ALL);
  while (1) {
    /* Process all waiting events without blocking */
    PSEvent* event;
    while ((event = PSEventTryAcquire()) != NULL) {
      ProcessEvent(event);
      PSEventRelease(event);
    }

    /* Render a frame, blocking until complete. */
    if (g_Context.bound) {
      Render();
    }
  }
  return 0;
}
