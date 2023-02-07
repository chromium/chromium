// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_
#define FUCHSIA_WEB_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_

#include <fuchsia/diagnostics/cpp/fidl.h>
#include <fuchsia/media/sessions2/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/interface_request.h>

#include "fuchsia_web/runners/cast/fidl/fidl/hlcpp/chromium/cast/cpp/fidl.h"

class ApplicationControllerImpl final
    : public chromium::cast::ApplicationController {
 public:
  ApplicationControllerImpl(fuchsia::web::Frame* frame,
                            chromium::cast::ApplicationContext* context);

  ApplicationControllerImpl(const ApplicationControllerImpl&) = delete;
  ApplicationControllerImpl& operator=(const ApplicationControllerImpl&) =
      delete;

  ~ApplicationControllerImpl() override;

 protected:
  // chromium::cast::ApplicationController implementation.
  void SetTouchInputEnabled(bool enable) override;
  void GetMediaPlayer(
      ::fidl::InterfaceRequest<fuchsia::media::sessions2::Player> request)
      override;
  void SetBlockMediaLoading(bool blocked) override;
  void GetPrivateMemorySize(GetPrivateMemorySizeCallback callback) override;

 private:
  fidl::Binding<chromium::cast::ApplicationController> binding_;
  fuchsia::web::Frame* const frame_;
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_
