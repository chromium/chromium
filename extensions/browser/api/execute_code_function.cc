// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_
#define EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_

#include "extensions/browser/api/execute_code_function.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/load_and_localize_file.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/mojom/css_origin.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/utils/content_script_utils.h"
#include "extensions/common/utils/extension_types_utils.h"

namespace {

// Error messages
const char kNoCodeOrFileToExecuteError[] = "No source code or file specified.";
const char kMoreThanOneValuesError[] =
    "Code and file should not be specified "
    "at the same time in the second argument.";
const char kBadFileEncodingError[] =
    "Could not load file '*' for content script. It isn't UTF-8 encoded.";
const char kCSSOriginForNonCSSError[] =
    "CSS origin should be specified only for CSS code.";

}

namespace extensions {

using api::extension_types::InjectDetails;

ExecuteCodeFunction::ExecuteCodeFunction() {
}

ExecuteCodeFunction::~ExecuteCodeFunction() {
}

void ExecuteCodeFunction::DidLoadAndLocalizeFile(
    const std::string& file,
    std::vector<std::unique_ptr<std::string>> data,
    std::optional<std::string> load_error) {
  if (load_error) {
    // TODO(viettrungluu): bug: there's no particular reason the path should be
    // UTF-8, in which case this may fail.
    Respond(Error(std::move(*load_error)));
    return;
  }

  DCHECK_EQ(1u, data.size());
  auto& file_data = data.front();
  if (!base::IsStringUTF8(*file_data)) {
    Respond(Error(ErrorUtils::FormatErrorMessage(kBadFileEncodingError, file)));
    return;
  }

  std::string error;
  if (!Execute(*file_data, &error))
    Respond(Error(std::move(error)));

  // If Execute() succeeds, the function will respond in
  // OnExecuteCodeFinished().
}

bool ExecuteCodeFunction::Execute(const std::string& code_string,
                                  std::string* error) {
  ScriptExecutor* executor = GetScriptExecutor(error);
  if (!executor)
    return false;

  // TODO(lazyboy): Set |error|?
  if (!extension() && !IsWebView())
    return false;

  DCHECK(!(ShouldInsertCSS() && ShouldRemoveCSS()));

  ScriptExecutor::FrameScope frame_scope =
      details_->all_frames.value_or(false) ? ScriptExecutor::INCLUDE_SUB_FRAMES
                                           : ScriptExecutor::SPECIFIED_FRAMES;

  root_frame_id_ =
      details_->frame_id.value_or(ExtensionApiFrameIdMap::kTopFrameId);

  ScriptExecutor::MatchAboutBlank match_about_blank =
      details_->match_about_blank.value_or(false)
          ? ScriptExecutor::MATCH_ABOUT_BLANK
          : ScriptExecutor::DONT_MATCH_ABOUT_BLANK;

  mojom::RunLocation run_at = ConvertRunLocation(details_->run_at);

  mojom::CSSOrigin css_origin = mojom::CSSOrigin::kAuthor;
  switch (details_->css_origin) {
    case api::extension_types::CSSOrigin::kNone:
    case api::extension_types::CSSOrigin::kAuthor:
      css_origin = mojom::CSSOrigin::kAuthor;
      break;
    case api::extension_types::CSSOrigin::kUser:
      css_origin = mojom::CSSOrigin::kUser;
      break;
  }

  mojom::CodeInjectionPtr injection;
  bool is_css_injection = ShouldInsertCSS() || ShouldRemoveCSS();
  if (is_css_injection) {
    std::optional<std::string> injection_key;
    if (host_id_.type == mojom::HostID::HostType::kExtensions) {
      injection_key = ScriptExecutor::GenerateInjectionKey(
          host_id_, script_url_, code_string);
    }
    mojom::CSSInjection::Operation operation =
        ShouldInsertCSS() ? mojom::CSSInjection::Operation::kAdd
                          : mojom::CSSInjection::Operation::kRemove;
    std::vector<mojom::CSSSourcePtr> sources;
    sources.push_back(
        mojom::CSSSource::New(code_string, std::move(injection_key)));
    injection = mojom::CodeInjection::NewCss(
        mojom::CSSInjection::New(std::move(sources), css_origin, operation));
  } else {
    bool wants_result = has_callback();
    std::vector<mojom::JSSourcePtr> sources;
    sources.push_back(mojom::JSSource::New(code_string, script_url_));
    // tabs.executeScript does not support waiting for promises (only
    // scripting.executeScript does).
    injection = mojom::CodeInjection::NewJs(mojom::JSInjection::New(
        std::move(sources), mojom::ExecutionWorld::kIsolated,
        /*world_id=*/std::nullopt,
        wants_result ? blink::mojom::WantResultOption::kWantResult
                     : blink::mojom::WantResultOption::kNoResult,
        user_gesture() ? blink::mojom::UserActivationOption::kActivate
                       : blink::mojom::UserActivationOption::kDoNotActivate,
        blink::mojom::PromiseResultOption::kDoNotWait));
  }

  executor->ExecuteScript(
      host_id_, std::move(injection), frame_scope, {root_frame_id_},
      match_about_blank, run_at,
      IsWebView() ? ScriptExecutor::WEB_VIEW_PROCESS
                  : ScriptExecutor::DEFAULT_PROCESS,
      GetWebViewSrc(),
      base::BindOnce(&ExecuteCodeFunction::OnExecuteCodeFinished, this));
  return true;
}

ExtensionFunction::ResponseAction ExecuteCodeFunction::Run() {
  InitResult init_result = Init();
  EXTENSION_FUNCTION_VALIDATE(init_result != VALIDATION_FAILURE);
  if (init_result == FAILURE)
    return RespondNow(Error(init_error_.value_or(kUnknownErrorDoNotUse)));

  if (!details_->code && !details_->file)
    return RespondNow(Error(kNoCodeOrFileToExecuteError));

  if (details_->code && details_->file)
    return RespondNow(Error(kMoreThanOneValuesError));

  if (details_->css_origin != api::extension_types::CSSOrigin::kNone &&
      !ShouldInsertCSS() && !ShouldRemoveCSS()) {
    return RespondNow(Error(kCSSOriginForNonCSSError));
  }

  std::string error;
  if (!CanExecuteScriptOnPage(&error))
    return RespondNow(Error(std::move(error)));

  if (details_->code) {
    if (!IsWebView() && extension()) {
      ExtensionsBrowserClient::Get()->NotifyExtensionApiTabExecuteScript(
          browser_context(), extension_id(), *details_->code);
    }

    if (!Execute(*details_->code, &error))
      return RespondNow(Error(std::move(error)));
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  DCHECK(details_->file);
  if (!LoadFile(*details_->file, &error))
    return RespondNow(Error(std::move(error)));

  // LoadFile will respond asynchronously later.
  return RespondLater();
}

bool ExecuteCodeFunction::LoadFile(const std::string& file,
                                   std::string* error) {
  ExtensionResource resource = extension()->GetResource(file);
  if (resource.extension_root().empty() || resource.relative_path().empty()) {
    *error = kNoCodeOrFileToExecuteError;
    return false;
  }
  script_url_ = extension()->GetResourceURL(file);

  bool might_require_localization = ShouldInsertCSS() || ShouldRemoveCSS();

  std::string relative_path = resource.relative_path().AsUTF8Unsafe();
  LoadAndLocalizeResources(
      *extension(), {std::move(resource)}, might_require_localization,
      script_parsing::GetMaxScriptLength(),
      base::BindOnce(&ExecuteCodeFunction::DidLoadAndLocalizeFile, this,
                     relative_path));

  return true;
}

void ExecuteCodeFunction::OnExecuteCodeFinished(
    std::vector<ScriptExecutor::FrameResult> results) {
  DCHECK(!results.empty());

  auto root_frame_result = base::ranges::find(
      results, root_frame_id_, &ScriptExecutor::FrameResult::frame_id);

  CHECK(root_frame_result != results.end(), base::NotFatalUntil::M130);

  // We just error out if we never injected in the root frame.
  // TODO(devlin): That's a bit odd, because other injections may have
  // succeeded. It seems like it might be worth passing back the values
  // anyway.
  if (!root_frame_result->error.empty()) {
    // If the frame never responded (e.g. the frame was removed or didn't
    // exist), we provide a different error message for backwards
    // compatibility.
    if (!root_frame_result->frame_responded) {
      root_frame_result->error =
          root_frame_id_ == ExtensionApiFrameIdMap::kTopFrameId
              ? "The tab was closed."
              : "The frame was removed.";
    }

    Respond(Error(std::move(root_frame_result->error)));
    return;
  }

  if (ShouldInsertCSS() || ShouldRemoveCSS()) {
    // insertCSS and removeCSS don't have a result argument.
    Respond(NoArguments());
    return;
  }

  // Place the root frame result at the beginning.
  std::iter_swap(root_frame_result, results.begin());
  base::Value::List result_list;
  for (auto& result : results) {
    if (result.error.empty())
      result_list.Append(std::move(result.value));
  }

  Respond(WithArguments(std::move(result_list)));
}

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_
