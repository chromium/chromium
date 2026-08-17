// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool_java_script_feature.h"

#import <string_view>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace actor {

namespace {

const char kScriptName[] = "attempt_form_filling_tool";
constexpr std::string_view kUniqueIdsKey = "uniqueIds";

mojom::ActionResultCode ToActionResultCode(int code) {
  auto result_code = static_cast<AttemptFormFillingToolResultCode>(code);
  switch (result_code) {
    case AttemptFormFillingToolResultCode::kOk:
      return mojom::ActionResultCode::kOk;
    case AttemptFormFillingToolResultCode::kCoordinatesOutOfBounds:
      return mojom::ActionResultCode::kCoordinatesOutOfBounds;
    case AttemptFormFillingToolResultCode::kInvalidDomNodeId:
      return mojom::ActionResultCode::kInvalidDomNodeId;
    case AttemptFormFillingToolResultCode::kTargetNotAutofillElement:
      return mojom::ActionResultCode::kFormFillingFieldNotFound;
    case AttemptFormFillingToolResultCode::kInvalidTarget:
    default:
      // TODO(crbug.com/505037793): Use a more appropriate ActionResultCode
      // for this case, or add Bling-specific errors more generally.
      return mojom::ActionResultCode::kArgumentsInvalid;
  }
}

base::expected<std::vector<uint32_t>, ToolExecutionResult>
ParseGetAutofillRendererIdsResult(const base::Value* result) {
  ToolExecutionResult tool_result =
      ParseJavaScriptResultWithResultCode(&ToActionResultCode, result);
  if (!tool_result.IsOk()) {
    return base::unexpected(tool_result);
  }

  const base::ListValue* unique_ids = result->GetDict().FindList(kUniqueIdsKey);
  if (!unique_ids) {
    // TODO(crbug.com/505037793): Use a more appropriate ActionResultCode
    // for this case, or add Bling-specific errors more generally.
    return base::unexpected(
        ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid,
                            /*requires_page_stabilization=*/false,
                            "List not found for autofill renderer ids."));
  }

  std::vector<uint32_t> renderer_ids;
  renderer_ids.reserve(unique_ids->size());
  for (const base::Value& item : *unique_ids) {
    if (!item.is_string()) {
      return base::unexpected(ToolExecutionResult(
          mojom::ActionResultCode::kFormFillingFieldNotFound));
    }
    const std::string& renderer_id = item.GetString();
    uint32_t id_num = 0;
    if (!base::StringToUint(renderer_id, &id_num) || id_num == 0) {
      return base::unexpected(ToolExecutionResult(
          mojom::ActionResultCode::kFormFillingFieldNotFound));
    }
    renderer_ids.push_back(id_num);
  }
  return renderer_ids;
}

}  // namespace

// static
AttemptFormFillingToolJavaScriptFeature*
AttemptFormFillingToolJavaScriptFeature::GetInstance() {
  static base::NoDestructor<AttemptFormFillingToolJavaScriptFeature> instance;
  return instance.get();
}

void AttemptFormFillingToolJavaScriptFeature::GetAutofillRendererIds(
    base::WeakPtr<web::WebFrame> target_frame,
    const std::vector<ActionTarget>& targets,
    GetAutofillRendererIdsCallback callback) {
  // Caller should have verified the existence of `target_frame`.
  CHECK(target_frame);
  CHECK(!targets.empty());
  base::ListValue targets_list;

  // Convert target to JavaScript dict.
  for (const ActionTarget& target : targets) {
    CHECK(target.is_valid());
    base::DictValue target_dict;
    if (target.node_id().has_value()) {
      target_dict.Set("nodeId",
                      static_cast<int>(target.node_id()->content_node_id));
    } else if (target.coordinate().has_value()) {
      target_dict.Set("x", target.coordinate()->x);
      target_dict.Set("y", target.coordinate()->y);
      target_dict.Set("pixelType",
                      static_cast<int>(target.coordinate()->pixel_type));
    }
    targets_list.Append(std::move(target_dict));
  }

  base::ListValue parameters;
  parameters.Append(std::move(targets_list));

  // Run JavaScript function.
  auto [cb_for_js, cb_for_error] = base::SplitOnceCallback(std::move(callback));
  bool sent = CallJavaScriptFunction(
      target_frame.get(), "attempt_form_filling.getAutofillRendererIds",
      parameters,
      base::BindOnce(
          [](GetAutofillRendererIdsCallback callback,
             const base::Value* result) {
            std::move(callback).Run(ParseGetAutofillRendererIdsResult(result));
          },
          std::move(cb_for_js)),
      base::Milliseconds(web::kJavaScriptFunctionCallDefaultTimeout));

  if (!sent) {
    std::move(cb_for_error)
        .Run(base::unexpected(ToolExecutionResult(
            InternalToolErrorCode::
                kJavascriptFeatureFailedToCallJavaScriptFunction)));
  }
}

AttemptFormFillingToolJavaScriptFeature::
    AttemptFormFillingToolJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

AttemptFormFillingToolJavaScriptFeature::
    ~AttemptFormFillingToolJavaScriptFeature() = default;

}  // namespace actor
