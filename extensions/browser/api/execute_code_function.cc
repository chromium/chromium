// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_
#define EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_

#include "extensions/browser/api/execute_code_function.h"

#include <utility>

#include "base/bind.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/load_and_localize_file.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"

namespace {

// Error messages
const char kNoCodeOrFileToExecuteError[] = "No source code or file specified.";
const char kMoreThanOneValuesError[] =
    "Code and file should not be specified "
    "at the same time in the second argument.";
const char kBadFileEncodingError[] =
    "Could not load file '*' for content script. It isn't UTF-8 encoded.";
const char kLoadFileError[] = "Failed to load file: \"*\". ";
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
    bool success,
    std::unique_ptr<std::string> data) {
  if (!success) {
    // TODO(viettrungluu): bug: there's no particular reason the path should be
    // UTF-8, in which case this may fail.
    Respond(Error(ErrorUtils::FormatErrorMessage(kLoadFileError, file)));
    return;
  }

  if (!base::IsStringUTF8(*data)) {
    Respond(Error(ErrorUtils::FormatErrorMessage(kBadFileEncodingError, file)));
    return;
  }

  std::string error;
  if (!Execute(*data, &error))
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

  auto action_type = UserScript::ActionType::ADD_JAVASCRIPT;
  if (ShouldInsertCSS())
    action_type = UserScript::ActionType::ADD_CSS;
  else if (ShouldRemoveCSS())
    action_type = UserScript::ActionType::REMOVE_CSS;

  ScriptExecutor::FrameScope frame_scope =
      details_->all_frames.get() && *details_->all_frames
          ? ScriptExecutor::INCLUDE_SUB_FRAMES
          : ScriptExecutor::SPECIFIED_FRAMES;

  int frame_id = details_->frame_id.get() ? *details_->frame_id
                                          : ExtensionApiFrameIdMap::kTopFrameId;

  ScriptExecutor::MatchAboutBlank match_about_blank =
      details_->match_about_blank.get() && *details_->match_about_blank
          ? ScriptExecutor::MATCH_ABOUT_BLANK
          : ScriptExecutor::DONT_MATCH_ABOUT_BLANK;

  UserScript::RunLocation run_at = UserScript::UNDEFINED;
  switch (details_->run_at) {
    case api::extension_types::RUN_AT_NONE:
    case api::extension_types::RUN_AT_DOCUMENT_IDLE:
      run_at = UserScript::DOCUMENT_IDLE;
      break;
    case api::extension_types::RUN_AT_DOCUMENT_START:
      run_at = UserScript::DOCUMENT_START;
      break;
    case api::extension_types::RUN_AT_DOCUMENT_END:
      run_at = UserScript::DOCUMENT_END;
      break;
  }
  CHECK_NE(UserScript::UNDEFINED, run_at);

  base::Optional<CSSOrigin> css_origin;
  if (details_->css_origin == api::extension_types::CSS_ORIGIN_USER)
    css_origin = CSS_ORIGIN_USER;
  else if (details_->css_origin == api::extension_types::CSS_ORIGIN_AUTHOR)
    css_origin = CSS_ORIGIN_AUTHOR;

  executor->ExecuteScript(
      host_id_, action_type, code_string, frame_scope, {frame_id},
      match_about_blank, run_at,
      IsWebView() ? ScriptExecutor::WEB_VIEW_PROCESS
                  : ScriptExecutor::DEFAULT_PROCESS,
      GetWebViewSrc(), script_url_, user_gesture(), css_origin,
      has_callback() ? ScriptExecutor::JSON_SERIALIZED_RESULT
                     : ScriptExecutor::NO_RESULT,
      base::Bind(&ExecuteCodeFunction::OnExecuteCodeFinished, this));
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

  if (details_->css_origin != api::extension_types::CSS_ORIGIN_NONE &&
      !ShouldInsertCSS() && !ShouldRemoveCSS()) {
    return RespondNow(Error(kCSSOriginForNonCSSError));
  }

  std::string error;
  if (!CanExecuteScriptOnPage(&error))
    return RespondNow(Error(std::move(error)));

  if (details_->code) {
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

  LoadAndLocalizeResource(
      *extension(), resource, might_require_localization,
      base::BindOnce(&ExecuteCodeFunction::DidLoadAndLocalizeFile, this,
                     resource.relative_path().AsUTF8Unsafe()));

  return true;
}

void ExecuteCodeFunction::OnExecuteCodeFinished(const std::string& error,
                                                const GURL& on_url,
                                                const base::ListValue& result) {
  if (!error.empty()) {
    Respond(Error(error));
    return;
  }

  // insertCSS and removeCSS don't have a result argument.
  Respond(ShouldInsertCSS() || ShouldRemoveCSS()
              ? NoArguments()
              : OneArgument(
                    base::Value::FromUniquePtrValue(result.CreateDeepCopy())));
}

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_
