// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/linux_output_window.h"

#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace cast {
namespace test {

LinuxOutputWindow::LinuxOutputWindow(int x_pos,
                                     int y_pos,
                                     int width,
                                     int height,
                                     const std::string& name) {
  CreateWindow(x_pos, y_pos, width, height, name);
}

LinuxOutputWindow::~LinuxOutputWindow() {
  if (display_ && window_) {
    XUnmapWindow(display_, window_);
    XDestroyWindow(display_, window_);
    XSync(display_, false);
    if (gc_)
      XFreeGC(display_, gc_);
    XCloseDisplay(display_);
  }
}

void LinuxOutputWindow::CreateWindow(int x_pos,
                                     int y_pos,
                                     int width,
                                     int height,
                                     const std::string& name) {
  display_ = XOpenDisplay(NULL);
  if (display_ == NULL) {
    // There's no point to continue if this happens: nothing will work anyway.
    VLOG(1) << "Failed to connect to X server: X environment likely broken";
    NOTREACHED();
  }

  int screen = DefaultScreen(display_);

  // Try to establish a 24-bit TrueColor display.
  // (our environment must allow this).
  XVisualInfo visual_info;
  if (XMatchVisualInfo(display_, screen, 24, TrueColor, &visual_info) == 0) {
    VLOG(1) << "Failed to establish 24-bit TrueColor in X environment.";
    NOTREACHED();
  }

  // Create suitable window attributes.
  XSetWindowAttributes window_attributes;
  window_attributes.colormap = XCreateColormap(
      display_, DefaultRootWindow(display_), visual_info.visual, AllocNone);
  window_attributes.event_mask = StructureNotifyMask | ExposureMask;
  window_attributes.background_pixel = 0;
  window_attributes.border_pixel = 0;

  unsigned long attribute_mask =
      CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

  window_ = XCreateWindow(display_,
                          DefaultRootWindow(display_),
                          x_pos,
                          y_pos,
                          width,
                          height,
                          0,
                          visual_info.depth,
                          InputOutput,
                          visual_info.visual,
                          attribute_mask,
                          &window_attributes);

  // Set window name.
  XStoreName(display_, window_, name.c_str());
  XSetIconName(display_, window_, name.c_str());

  // Make x report events for mask.
  XSelectInput(display_, window_, StructureNotifyMask);

  // Map the window to the display.
  XMapWindow(display_, window_);

  // Wait for map event.
  XEvent event;
  do {
    XNextEvent(display_, &event);
  } while (event.type != MapNotify || event.xmap.event != window_);

  gc_ = XCreateGC(display_, window_, 0, 0);

  // create shared memory image
  image_ = XShmCreateImage(
      display_, CopyFromParent, 24, ZPixmap, NULL, &shminfo_, width, height);
  shminfo_.shmid = shmget(
      IPC_PRIVATE, (image_->bytes_per_line * image_->height), IPC_CREAT | 0777);
  shminfo_.shmaddr = image_->data = (char*)shmat(shminfo_.shmid, 0, 0);
  if (image_->data == reinterpret_cast<char*>(-1)) {
    VLOG(1) << "XShmCreateImage failed";
    NOTREACHED();
  }
  shminfo_.readOnly = false;

  // Attach image to display.
  if (!XShmAttach(display_, &shminfo_)) {
    VLOG(1) << "XShmAttach failed";
    NOTREACHED();
  }
  XSync(display_, false);
}

void LinuxOutputWindow::RenderFrame(const media::VideoFrame& video_frame) {
  const gfx::Size damage_size(
      std::min(video_frame.visible_rect().width(), image_->width),
      std::min(video_frame.visible_rect().height(), image_->height));

  if (damage_size.width() < image_->width ||
      damage_size.height() < image_->height)
    memset(image_->data, 0x00, image_->bytes_per_line * image_->height);

  if (!damage_size.IsEmpty()) {
    libyuv::I420ToARGB(video_frame.visible_data(VideoFrame::kYPlane),
                       video_frame.stride(VideoFrame::kYPlane),
                       video_frame.visible_data(VideoFrame::kUPlane),
                       video_frame.stride(VideoFrame::kUPlane),
                       video_frame.visible_data(VideoFrame::kVPlane),
                       video_frame.stride(VideoFrame::kVPlane),
                       reinterpret_cast<uint8_t*>(image_->data),
                       image_->bytes_per_line, damage_size.width(),
                       damage_size.height());
  }

  // Place image in window.
  XShmPutImage(display_,
               window_,
               gc_,
               image_,
               0,
               0,
               0,
               0,
               image_->width,
               image_->height,
               true);

  // Very important for the image to update properly!
  XSync(display_, false);
}

}  // namespace test
}  // namespace cast
}  // namespace media
