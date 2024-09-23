// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/frame_test_util.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "fuchsia_web/common/test/fit_adapter.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"

bool LoadUrlAndExpectResponse(
    fuchsia::web::NavigationController* navigation_controller,
    fuchsia::web::LoadUrlParams load_url_params,
    std::string_view url) {
  CHECK(navigation_controller);
  base::test::TestFuture<fuchsia::web::NavigationController_LoadUrl_Result>
      result;
  navigation_controller->LoadUrl(std::string(url), std::move(load_url_params),
                                 CallbackToFitFunction(result.GetCallback()));
  CHECK(result.Wait());
  return result.Get().is_response();
}

bool LoadUrlAndExpectResponse(
    const fuchsia::web::NavigationControllerPtr& controller,
    fuchsia::web::LoadUrlParams params,
    std::string_view url) {
  return LoadUrlAndExpectResponse(controller.get(), std::move(params), url);
}

std::optional<base::Value> ExecuteJavaScript(fuchsia::web::Frame* frame,
                                             std::string_view script) {
  base::test::TestFuture<fuchsia::web::Frame_ExecuteJavaScript_Result> result;
  frame->ExecuteJavaScript({"*"}, base::MemBufferFromString(script, "test"),
                           CallbackToFitFunction(result.GetCallback()));

  if (!result.Wait() || !result.Get().is_response())
    return {};

  std::optional<std::string> result_json =
      base::StringFromMemBuffer(result.Get().response().result);
  if (!result_json) {
    return {};
  }

  return base::JSONReader::Read(*result_json);
}

fuchsia::web::LoadUrlParams CreateLoadUrlParamsWithUserActivation() {
  fuchsia::web::LoadUrlParams load_url_params;
  load_url_params.set_was_user_activated(true);
  return load_url_params;
}

fuchsia::web::WebMessage CreateWebMessageWithMessagePortRequest(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request,
    fuchsia::mem::Buffer buffer) {
  fuchsia::web::OutgoingTransferable outgoing;
  outgoing.set_message_port(std::move(message_port_request));

  std::vector<fuchsia::web::OutgoingTransferable> outgoing_vector;
  outgoing_vector.push_back(std::move(outgoing));

  fuchsia::web::WebMessage web_message;
  web_message.set_outgoing_transfer(std::move(outgoing_vector));
  web_message.set_data(std::move(buffer));
  return web_message;
}
