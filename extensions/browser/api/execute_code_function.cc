// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_
#define EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_

#include "extensions/browser/api/execute_code_function.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/file_reader.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/message_bundle.h"
#include "net/base/filename_util.h"
#include "ui/base/resource/resource_bundle.h"

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

void ExecuteCodeFunction::GetFileURLAndMaybeLocalizeInBackground(
    const std::string& extension_id,
    const base::FilePath& extension_path,
    const std::string& extension_default_locale,
    bool might_require_localization,
    std::string* data) {
  // TODO(karandeepb): Limit scope of ScopedBlockingCall.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // TODO(devlin): FilePathToFileURL() doesn't need to be done on a blocking
  // task runner, so we could do that on the UI thread and then avoid the hop
  // if we don't need localization.
  file_url_ = net::FilePathToFileURL(resource_.GetFilePath());

  if (!might_require_localization)
    return;

  bool needs_message_substituion =
      data->find(extensions::MessageBundle::kMessageBegin) != std::string::npos;
  if (!needs_message_substituion)
    return;

  std::unique_ptr<SubstitutionMap> localization_messages(
      file_util::LoadMessageBundleSubstitutionMap(extension_path, extension_id,
                                                  extension_default_locale));

  std::string error;
  MessageBundle::ReplaceMessagesWithExternalDictionary(*localization_messages,
                                                       data, &error);
}

std::unique_ptr<std::string>
ExecuteCodeFunction::GetFileURLAndLocalizeComponentResourceInBackground(
    std::unique_ptr<std::string> data,
    const std::string& extension_id,
    const base::FilePath& extension_path,
    const std::string& extension_default_locale,
    bool might_require_localization) {
  GetFileURLAndMaybeLocalizeInBackground(
      extension_id, extension_path, extension_default_locale,
      might_require_localization, data.get());

  return data;
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
    Respond(Error(error));

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

  ScriptExecutor::ScriptType script_type = ScriptExecutor::JAVASCRIPT;
  if (ShouldInsertCSS())
    script_type = ScriptExecutor::CSS;

  ScriptExecutor::FrameScope frame_scope =
      details_->all_frames.get() && *details_->all_frames
          ? ScriptExecutor::INCLUDE_SUB_FRAMES
          : ScriptExecutor::SINGLE_FRAME;

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
      host_id_, script_type, code_string, frame_scope, frame_id,
      match_about_blank, run_at,
      IsWebView() ? ScriptExecutor::WEB_VIEW_PROCESS
                  : ScriptExecutor::DEFAULT_PROCESS,
      GetWebViewSrc(), file_url_, user_gesture(), css_origin,
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
      !ShouldInsertCSS()) {
    return RespondNow(Error(kCSSOriginForNonCSSError));
  }

  std::string error;
  if (!CanExecuteScriptOnPage(&error))
    return RespondNow(Error(error));

  if (details_->code) {
    if (!Execute(*details_->code, &error))
      return RespondNow(Error(error));
    return did_respond() ? AlreadyResponded() : RespondLater();
  }

  DCHECK(details_->file);
  if (!LoadFile(*details_->file, &error))
    return RespondNow(Error(error));

  // LoadFile will respond asynchronously later.
  return RespondLater();
}

bool ExecuteCodeFunction::LoadFile(const std::string& file,
                                   std::string* error) {
  resource_ = extension()->GetResource(file);

  if (resource_.extension_root().empty() || resource_.relative_path().empty()) {
    *error = kNoCodeOrFileToExecuteError;
    return false;
  }

  const std::string& extension_id = extension()->id();
  base::FilePath extension_path = extension()->path();
  std::string extension_default_locale;
  extension()->manifest()->GetString(manifest_keys::kDefaultLocale,
                                     &extension_default_locale);
  // TODO(lazyboy): |extension_id| should not be empty(), turn this into a
  // DCHECK.
  bool might_require_localization = ShouldInsertCSS() && !extension_id.empty();

  int resource_id = 0;
  const ComponentExtensionResourceManager*
      component_extension_resource_manager =
          ExtensionsBrowserClient::Get()
              ->GetComponentExtensionResourceManager();
  if (component_extension_resource_manager &&
      component_extension_resource_manager->IsComponentExtensionResource(
          resource_.extension_root(), resource_.relative_path(),
          &resource_id)) {
    auto data = std::make_unique<std::string>(
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            resource_id));

    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&ExecuteCodeFunction::
                           GetFileURLAndLocalizeComponentResourceInBackground,
                       this, std::move(data), extension_id, extension_path,
                       extension_default_locale, might_require_localization),
        base::BindOnce(&ExecuteCodeFunction::DidLoadAndLocalizeFile, this,
                       resource_.relative_path().AsUTF8Unsafe(),
                       true /* We assume this call always succeeds */));
  } else {
    FileReader::OptionalFileSequenceTask get_file_and_l10n_callback =
        base::BindOnce(
            &ExecuteCodeFunction::GetFileURLAndMaybeLocalizeInBackground, this,
            extension_id, extension_path, extension_default_locale,
            might_require_localization);

    auto file_reader = base::MakeRefCounted<FileReader>(
        resource_, std::move(get_file_and_l10n_callback),
        base::BindOnce(&ExecuteCodeFunction::DidLoadAndLocalizeFile, this,
                       resource_.relative_path().AsUTF8Unsafe()));
    file_reader->Start();
  }

  return true;
}

void ExecuteCodeFunction::OnExecuteCodeFinished(const std::string& error,
                                                const GURL& on_url,
                                                const base::ListValue& result) {
  if (!error.empty()) {
    Respond(Error(error));
    return;
  }

  // insertCSS doesn't have a result argument.
  Respond(ShouldInsertCSS() ? NoArguments()
                            : OneArgument(result.CreateDeepCopy()));
}

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_EXECUTE_CODE_FUNCTION_IMPL_H_
