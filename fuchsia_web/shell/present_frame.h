// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_SHELL_PRESENT_FRAME_H_
#define FUCHSIA_WEB_SHELL_PRESENT_FRAME_H_

#include <fuchsia/element/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>

// Presents the given frame by setting up the necessary views, connecting to a
// fuchsia view presentation protocol, and forwarding the given annotation
// controller and annotations.
void PresentFrame(fuchsia::web::Frame* frame,
                  fidl::InterfaceHandle<fuchsia::element::AnnotationController>
                      annotation_controller);

#endif  // FUCHSIA_WEB_SHELL_PRESENT_FRAME_H_
