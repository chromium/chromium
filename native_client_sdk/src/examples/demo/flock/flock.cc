// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_size.h"

#include "ppapi_simple/ps_context_2d.h"

#include "goose.h"
#include "sprite.h"
#include "vector2.h"


namespace {
  // The goose sprites rotate in increments of 5 degrees.
const double kGooseHeadingIncrement = (5.0 * M_PI) / 180.0;
}  // namespace

struct ImageFormat {
  int width;
  int height;
  int channels;
};

Sprite* g_goose_sprite;
std::vector<Goose> g_geese;
std::vector<Vector2> g_attractors;


void ResetFlock(PSContext2D_t* ctx, size_t count) {
  Vector2 center(0.5 * ctx->width, 0.5 * ctx->height);

  g_geese.resize(count);
  for (size_t i = 0; i < count; i++) {
    double dx = (double) rand() / (double) RAND_MAX;
    double dy = (double) rand() / (double) RAND_MAX;
    g_geese[i] = Goose(center, Vector2(dx, dy));
  }
}

void Render(PSContext2D_t* ctx) {
  PSContext2DGetBuffer(ctx);
  const size_t num_geese = g_geese.size();

  if (NULL == g_goose_sprite) return;
  if (NULL == ctx->data) return;

  // Clear to WHITE
  memset(ctx->data, 0xFF, ctx->stride * ctx->height);

  int32_t sprite_side_length = g_goose_sprite->size().height();
  pp::Rect sprite_src_rect(0, 0, sprite_side_length, sprite_side_length);
  pp::Rect canvas_bounds(pp::Size(ctx->width, ctx->height));


  // Run the simulation for each goose.
  for (size_t i = 0; i < num_geese; i++) {
    Goose& goose = g_geese[i];

    // Update position and orientation
    goose.SimulationTick(g_geese, g_attractors, canvas_bounds);
    pp::Point dest_point(goose.location().x() - sprite_side_length / 2,
                         goose.location().y() - sprite_side_length / 2);

    // Compute image to use
    double heading = goose.velocity().Heading();
    if (heading < 0.0)
      heading = M_PI * 2.0 + heading;

    int32_t sprite_index =
        static_cast<int32_t>(heading / kGooseHeadingIncrement);

    sprite_src_rect.set_x(sprite_index * sprite_side_length);
    g_goose_sprite->CompositeFromRectToPoint(
        sprite_src_rect,
        ctx->data, canvas_bounds, 0,
        dest_point);
  }

  PSContext2DSwapBuffer(ctx);
}

/*
 * Starting point for the module.  We do not use main since it would
 * collide with main in libppapi_cpp.
 */
int main(int argc, char *argv[]) {
  ImageFormat fmt;
  uint32_t* buffer;
  size_t len;

  PSEventSetFilter(PSE_ALL);

  // Mount the images directory as an HTTP resource.
  mount("images", "/images", "httpfs", 0, "");

  FILE* fp = fopen("/images/flock_green.raw", "rb");
  fread(&fmt, sizeof(fmt), 1, fp);

  len = fmt.width * fmt.height * fmt.channels;
  buffer = new uint32_t[len];
  fread(buffer, 1, len, fp);
  fclose(fp);

  g_goose_sprite = new Sprite(buffer, pp::Size(fmt.width, fmt.height), 0);

  PSContext2D_t* ctx = PSContext2DAllocate(PP_IMAGEDATAFORMAT_BGRA_PREMUL);
  ResetFlock(ctx, 50);
  while (1) {
    PSEvent* event;

    // Consume all available events
    while ((event = PSEventTryAcquire()) != NULL) {
      PSContext2DHandleEvent(ctx, event);
      PSEventRelease(event);
    }

    if (ctx->bound) {
      Render(ctx);
    } else {
      // If not bound, wait for an event which may signal a context becoming
      // available, instead of spinning.
      event = PSEventWaitAcquire();
      if (PSContext2DHandleEvent(ctx, event))
        ResetFlock(ctx, 50);
      PSEventRelease(event);
    }
  }

  return 0;
}
