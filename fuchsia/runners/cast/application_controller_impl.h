// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_
#define FUCHSIA_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/macros.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"

class ApplicationControllerImpl : public chromium::cast::ApplicationController {
 public:
  ApplicationControllerImpl(
      fuchsia::web::Frame* frame,
      fidl::InterfaceHandle<chromium::cast::ApplicationControllerReceiver>
          receiver);
  ~ApplicationControllerImpl() final;

 protected:
  void SetTouchInputEnabled(bool enable) override;

 private:
  fidl::Binding<chromium::cast::ApplicationController> binding_;
  fuchsia::web::Frame* const frame_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationControllerImpl);
};

#endif  // FUCHSIA_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_
