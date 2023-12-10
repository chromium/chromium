// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_
#define FUCHSIA_WEB_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_

#include <fidl/chromium.cast/cpp/fidl.h>
#include <fuchsia/media/sessions2/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/interface_request.h>

#include <optional>

class ApplicationControllerImpl final
    : public fidl::Server<chromium_cast::ApplicationController> {
 public:
  // `trace_flow_id` is used by the controller to report media blocking trace
  // event as a part of the application flow.
  ApplicationControllerImpl(
      fuchsia::web::Frame* frame,
      fidl::Client<chromium_cast::ApplicationContext>& context,
      uint64_t trace_flow_id);

  ApplicationControllerImpl(const ApplicationControllerImpl&) = delete;
  ApplicationControllerImpl& operator=(const ApplicationControllerImpl&) =
      delete;

  ~ApplicationControllerImpl() override;

 protected:
  // chromium_cast::ApplicationController implementation.
  void SetTouchInputEnabled(
      SetTouchInputEnabledRequest& request,
      SetTouchInputEnabledCompleter::Sync& completer) override;
  void GetMediaPlayer(GetMediaPlayerRequest& request,
                      GetMediaPlayerCompleter::Sync& completer) override;
  void SetBlockMediaLoading(
      SetBlockMediaLoadingRequest& request,
      SetBlockMediaLoadingCompleter::Sync& completer) override;
  void GetPrivateMemorySize(
      GetPrivateMemorySizeCompleter::Sync& completer) override;

 private:
  std::optional<fidl::ServerBinding<chromium_cast::ApplicationController>>
      binding_;
  fuchsia::web::Frame* const frame_;
  const uint64_t trace_flow_id_;
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_APPLICATION_CONTROLLER_IMPL_H_
