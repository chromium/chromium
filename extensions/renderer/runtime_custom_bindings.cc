// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/runtime_custom_bindings.h"

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/common/view_type_util.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

RuntimeCustomBindings::RuntimeCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

RuntimeCustomBindings::~RuntimeCustomBindings() = default;

void RuntimeCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "GetExtensionViews",
      base::BindRepeating(&RuntimeCustomBindings::GetExtensionViews,
                          base::Unretained(this)));
}

void RuntimeCustomBindings::GetExtensionViews(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 3);
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsInt32());
  CHECK(args[2]->IsString());

  // |browser_window_id| == extension_misc::kUnknownWindowId means getting
  // all views for the current extension.
  int browser_window_id = args[0].As<v8::Int32>()->Value();
  int tab_id = args[1].As<v8::Int32>()->Value();

  std::string view_type_string =
      base::ToUpperASCII(*v8::String::Utf8Value(args.GetIsolate(), args[2]));
  // |view_type| == mojom::ViewType::kInvalid means getting any type of
  // views.
  mojom::ViewType view_type = mojom::ViewType::kInvalid;
  bool parsed_view_type = GetViewTypeFromString(view_type_string, &view_type);
  if (!parsed_view_type)
    CHECK_EQ("ALL", view_type_string);

  const ExtensionId& extension_id = context()->GetExtensionID();
  if (extension_id.empty())
    return;

  // We ignore iframes here. (Returning subframes can cause broken behavior by
  // treating an app window's iframe as its main frame, and maybe other
  // nastiness).
  // TODO(devlin): Why wouldn't we just account for that? It seems like there
  // can be reasons to want to access just a frame - especially with isolated
  // extension frames in web pages.
  v8::Local<v8::Array> v8_views = ExtensionFrameHelper::GetV8MainFrames(
      context()->v8_context(), extension_id, browser_window_id, tab_id,
      view_type);

  args.GetReturnValue().Set(v8_views);
}

}  // namespace extensions
