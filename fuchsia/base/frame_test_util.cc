// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/frame_test_util.h"

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_navigation_listener.h"

namespace cr_fuchsia {

bool LoadUrlAndExpectResponse(
    fuchsia::web::NavigationController* navigation_controller,
    fuchsia::web::LoadUrlParams load_url_params,
    base::StringPiece url) {
  DCHECK(navigation_controller);
  base::RunLoop run_loop;
  ResultReceiver<fuchsia::web::NavigationController_LoadUrl_Result> result(
      run_loop.QuitClosure());
  navigation_controller->LoadUrl(
      url.as_string(), std::move(load_url_params),
      CallbackToFitFunction(result.GetReceiveCallback()));
  run_loop.Run();
  return result->is_response();
}

base::Optional<base::Value> ExecuteJavaScript(fuchsia::web::Frame* frame,
                                              base::StringPiece script) {
  base::RunLoop run_loop;
  ResultReceiver<fuchsia::web::Frame_ExecuteJavaScript_Result> result(
      run_loop.QuitClosure());
  frame->ExecuteJavaScript({"*"}, MemBufferFromString(script, "test"),
                           CallbackToFitFunction(result.GetReceiveCallback()));
  run_loop.Run();

  if (!result.has_value() || !result->is_response())
    return {};

  std::string result_json;
  if (!StringFromMemBuffer(result->response().result, &result_json)) {
    return {};
  }

  return base::JSONReader::Read(result_json);
}

}  // namespace cr_fuchsia
