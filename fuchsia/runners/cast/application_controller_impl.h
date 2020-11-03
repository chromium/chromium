// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_
#define FUCHSIA_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_

#include <fuchsia/diagnostics/cpp/fidl.h>
#include <fuchsia/media/sessions2/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/interface_request.h>

#include "base/macros.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"

class ApplicationControllerImpl : public chromium::cast::ApplicationController {
 public:
  ApplicationControllerImpl(fuchsia::web::Frame* frame,
                            chromium::cast::ApplicationContext* context);
  ~ApplicationControllerImpl() final;

 protected:
  // chromium::cast::ApplicationController implementation.
  void SetTouchInputEnabled(bool enable) final;
  void GetMediaPlayer(
      ::fidl::InterfaceRequest<fuchsia::media::sessions2::Player> request)
      final;
  void SetBlockMediaLoading(bool blocked) override;
  void GetPrivateMemorySize(GetPrivateMemorySizeCallback callback) override;

 private:
  fidl::Binding<chromium::cast::ApplicationController> binding_;
  fuchsia::web::Frame* const frame_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationControllerImpl);
};

#endif  // FUCHSIA_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_
