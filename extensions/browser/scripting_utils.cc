// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/scripting_utils.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/script_executor.h"
#include "extensions/browser/scripting_constants.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/api/scripts_internal.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "extensions/common/utils/content_script_utils.h"

namespace extensions::scripting {

namespace {

constexpr char kEmptyScriptIdError[] = "Script's ID must not be empty";
constexpr char kFilesExceededSizeLimitError[] =
    "Scripts could not be loaded because '*' exceeds the maximum script size "
    "or the extension's maximum total script size.";
constexpr char kNonExistentScriptIdError[] = "Nonexistent script ID '*'";
// Key corresponding to the set of URL patterns from the extension's persistent
// dynamic content scripts.
constexpr const char kPrefPersistentScriptURLPatterns[] =
    "persistent_script_url_patterns";
constexpr char kReservedScriptIdPrefixError[] =
    "Script's ID '*' must not start with '*'";
constexpr char kInvalidTabIdError[] = "No tab with id: *";
constexpr char kInvalidDocumentIdError[] = "Invalid document id *";
constexpr char kInvalidDocumentIdForTabError[] =
    "No document with id * in tab with id *";
constexpr char kInvalidFrameIdError[] = "No frame with id * in tab with id *";
constexpr char kInvalidAllFramesTargetError[] =
    "Cannot specify 'allFrames' if either 'frameIds' or 'documentIds' is "
    "specified.";
constexpr char kInvalidTargetIdsError[] =
    "Cannot specify both 'frameIds' and 'documentIds'.";

// Returns an error message string for when an extension cannot access a page it
// is attempting to.
std::string GetCannotAccessPageErrorMessage(const PermissionsData& permissions,
                                            const GURL& url) {
  if (permissions.HasAPIPermission(mojom::APIPermissionID::kTab)) {
    return ErrorUtils::FormatErrorMessage(
        manifest_errors::kCannotAccessPageWithUrl, url.spec());
  }
  return manifest_errors::kCannotAccessPage;
}

// Collects the frames for injection. Method will return false if an error is
// encountered.
bool CollectFramesForInjection(const scripting::InjectionTarget& target,
                               content::WebContents* tab,
                               std::set<int>& frame_ids,
                               std::set<content::RenderFrameHost*>& frames,
                               std::string* error_out) {
  if (target.document_ids) {
    for (const auto& id : *target.document_ids) {
      ExtensionApiFrameIdMap::DocumentId document_id =
          ExtensionApiFrameIdMap::DocumentIdFromString(id);

      if (!document_id) {
        *error_out =
            ErrorUtils::FormatErrorMessage(kInvalidDocumentIdError, id.c_str());
        return false;
      }

      content::RenderFrameHost* frame =
          ExtensionApiFrameIdMap::Get()->GetRenderFrameHostByDocumentId(
              document_id);

      // If the frame was not found or it matched another tab reject this
      // request.
      if (!frame || content::WebContents::FromRenderFrameHost(frame) != tab) {
        *error_out = ErrorUtils::FormatErrorMessage(
            kInvalidDocumentIdForTabError, id.c_str(),
            base::NumberToString(target.tab_id));
        return false;
      }

      // Convert the documentId into a frameId since the content will be
      // injected synchronously.
      frame_ids.insert(ExtensionApiFrameIdMap::GetFrameId(frame));
      frames.insert(frame);
    }
  } else {
    if (target.frame_ids) {
      frame_ids.insert(target.frame_ids->begin(), target.frame_ids->end());
    } else {
      frame_ids.insert(ExtensionApiFrameIdMap::kTopFrameId);
    }

    for (int frame_id : frame_ids) {
      content::RenderFrameHost* frame =
          ExtensionApiFrameIdMap::GetRenderFrameHostById(tab, frame_id);
      if (!frame) {
        *error_out = ErrorUtils::FormatErrorMessage(
            kInvalidFrameIdError, base::NumberToString(frame_id),
            base::NumberToString(target.tab_id));
        return false;
      }
      frames.insert(frame);
    }
  }
  return true;
}

// Returns true if the `permissions` allow for injection into the given `frame`.
// If false, populates `error`.
bool HasPermissionToInjectIntoFrame(const PermissionsData& permissions,
                                    int tab_id,
                                    content::RenderFrameHost* frame,
                                    std::string* error) {
  GURL committed_url = frame->GetLastCommittedURL();
  if (committed_url.is_empty()) {
    if (!frame->IsInPrimaryMainFrame()) {
      // We can't check the pending URL for subframes from the //chrome layer.
      // Assume the injection is allowed; the renderer has additional checks
      // later on.
      return true;
    }
    // Unknown URL, e.g. because no load was committed yet. In this case we look
    // for any pending entry on the NavigationController associated with the
    // WebContents for the frame.
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(frame);
    content::NavigationEntry* pending_entry =
        web_contents->GetController().GetPendingEntry();
    if (!pending_entry) {
      *error = manifest_errors::kCannotAccessPage;
      return false;
    }
    GURL pending_url = pending_entry->GetURL();
    if (pending_url.SchemeIsHTTPOrHTTPS() &&
        !permissions.CanAccessPage(pending_url, tab_id, error)) {
      // This catches the majority of cases where an extension tried to inject
      // on a newly-created navigating tab, saving us a potentially-costly IPC
      // and, maybe, slightly reducing (but not by any stretch eliminating) an
      // attack surface.
      *error = GetCannotAccessPageErrorMessage(permissions, pending_url);
      return false;
    }

    // Otherwise allow for now. The renderer has additional checks and will
    // fail the injection if needed.
    return true;
  }

  // TODO(devlin): Add more schemes here, in line with
  // https://crbug.com/55084.
  if (committed_url.SchemeIs(url::kAboutScheme) ||
      committed_url.SchemeIs(url::kDataScheme)) {
    url::Origin origin = frame->GetLastCommittedOrigin();
    const url::SchemeHostPort& tuple_or_precursor_tuple =
        origin.GetTupleOrPrecursorTupleIfOpaque();
    if (!tuple_or_precursor_tuple.IsValid()) {
      *error = GetCannotAccessPageErrorMessage(permissions, committed_url);
      return false;
    }

    committed_url = tuple_or_precursor_tuple.GetURL();
  }

  return permissions.CanAccessPage(committed_url, tab_id, error);
}

}  // namespace

InjectionTarget::InjectionTarget() : tab_id(-1) {}

InjectionTarget::InjectionTarget(InjectionTarget&& other) = default;

InjectionTarget::~InjectionTarget() = default;

std::string AddPrefixToDynamicScriptId(const std::string& script_id,
                                       UserScript::Source source) {
  std::string prefix;
  switch (source) {
    case UserScript::Source::kDynamicContentScript:
      prefix = UserScript::kDynamicContentScriptPrefix;
      break;
    case UserScript::Source::kDynamicUserScript:
      prefix = UserScript::kDynamicUserScriptPrefix;
      break;
    case UserScript::Source::kStaticContentScript:
    case UserScript::Source::kWebUIScript:
      NOTREACHED();
  }

  return prefix + script_id;
}

bool IsScriptIdValid(const std::string& script_id, std::string* error) {
  if (script_id.empty()) {
    *error = kEmptyScriptIdError;
    return false;
  }

  if (script_id[0] == UserScript::kReservedScriptIDPrefix) {
    *error = ErrorUtils::FormatErrorMessage(
        kReservedScriptIdPrefixError, script_id,
        std::string(1, UserScript::kReservedScriptIDPrefix));
    return false;
  }

  return true;
}

bool ScriptsShouldBeAllowedInIncognito(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context) {
  // Note: We explicitly use `util::IsIncognitoEnabled()` (and not
  // `ExtensionFunction::include_incognito_information()`) since the latter
  // excludes the on-the-record context of a split-mode extension. Since user
  // scripts are shared across profiles, we should use the overall setting for
  // the extension.
  return util::IsIncognitoEnabled(extension_id, browser_context);
}

bool RemoveScripts(
    const std::optional<std::vector<std::string>>& ids,
    UserScript::Source source,
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    ExtensionUserScriptLoader::DynamicScriptsModifiedCallback remove_callback,
    std::string* error) {
  ExtensionUserScriptLoader* loader =
      ExtensionSystem::Get(browser_context)
          ->user_script_manager()
          ->GetUserScriptLoaderForExtension(extension_id);

  // Remove all scripts if ids are not provided. This doesn't include when ids
  // has a value, but it's empty.
  if (!ids.has_value()) {
    loader->ClearDynamicScripts(source, std::move(remove_callback));
    return true;
  }

  std::set<std::string> ids_to_remove;
  std::set<std::string> existing_script_ids =
      loader->GetDynamicScriptIDs(source);

  for (const auto& id : *ids) {
    if (!scripting::IsScriptIdValid(id, error)) {
      return false;
    }

    // Add the dynamic script prefix to `provided_id` before checking against
    // `existing_script_ids`.
    std::string id_with_prefix =
        scripting::AddPrefixToDynamicScriptId(id, source);
    if (!base::Contains(existing_script_ids, id_with_prefix)) {
      *error =
          ErrorUtils::FormatErrorMessage(kNonExistentScriptIdError, id.c_str());
      return false;
    }

    ids_to_remove.insert(id_with_prefix);
  }

  loader->RemoveDynamicScripts(std::move(ids_to_remove),
                               std::move(remove_callback));
  return true;
}

URLPatternSet GetPersistentScriptURLPatterns(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id) {
  URLPatternSet patterns;
  ExtensionPrefs::Get(browser_context)
      ->ReadPrefAsURLPatternSet(extension_id, kPrefPersistentScriptURLPatterns,
                                &patterns,
                                UserScript::ValidUserScriptSchemes());

  return patterns;
}

void SetPersistentScriptURLPatterns(content::BrowserContext* browser_context,
                                    const ExtensionId& extension_id,
                                    const URLPatternSet& patterns) {
  ExtensionPrefs::Get(browser_context)
      ->SetExtensionPrefURLPatternSet(
          extension_id, kPrefPersistentScriptURLPatterns, patterns);
}

void ClearPersistentScriptURLPatterns(content::BrowserContext* browser_context,
                                      const ExtensionId& extension_id) {
  ExtensionPrefs::Get(browser_context)
      ->UpdateExtensionPref(extension_id, kPrefPersistentScriptURLPatterns,
                            std::nullopt);
}

ValidateScriptsResult ValidateParsedScriptsOnFileThread(
    ExtensionResource::SymlinkPolicy symlink_policy,
    UserScriptList scripts) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());

  // Validate that claimed script resources actually exist, and are UTF-8
  // encoded.
  std::string error;
  std::vector<InstallWarning> warnings;
  bool are_script_files_valid = script_parsing::ValidateFileSources(
      scripts, symlink_policy, &error, &warnings);

  // Script files over the per script/extension size limit are recorded as
  // warnings. However, for this case we should treat "install warnings" as
  // errors by turning this call into a no-op and returning an error.
  if (!warnings.empty() && error.empty()) {
    error = ErrorUtils::FormatErrorMessage(kFilesExceededSizeLimitError,
                                           warnings[0].specific);
    are_script_files_valid = false;
  }

  return std::make_pair(std::move(scripts), are_script_files_valid
                                                ? std::nullopt
                                                : std::make_optional(error));
}

bool CanAccessTarget(const PermissionsData& permissions,
                     const scripting::InjectionTarget& target,
                     content::BrowserContext* browser_context,
                     bool include_incognito_information,
                     ScriptExecutor** script_executor_out,
                     ScriptExecutor::FrameScope* frame_scope_out,
                     std::set<int>* frame_ids_out,
                     std::string* error_out) {
  ExtensionsBrowserClient* browser_client = ExtensionsBrowserClient::Get();
  content::WebContents* web_contents = nullptr;
  if (!browser_client->IsValidTabId(browser_context, target.tab_id,
                                    include_incognito_information,
                                    &web_contents)) {
    *error_out = ErrorUtils::FormatErrorMessage(
        kInvalidTabIdError, base::NumberToString(target.tab_id));
    return false;
  }

  ScriptExecutor* script_executor =
      browser_client->GetScriptExecutorForTab(*web_contents);
  if (!script_executor) {
    *error_out = ErrorUtils::FormatErrorMessage(
        kInvalidTabIdError, base::NumberToString(target.tab_id));
    return false;
  }

  if (target.all_frames.value_or(false) &&
      (target.frame_ids || target.document_ids)) {
    *error_out = kInvalidAllFramesTargetError;
    return false;
  }

  if (target.frame_ids && target.document_ids) {
    *error_out = kInvalidTargetIdsError;
    return false;
  }

  ScriptExecutor::FrameScope frame_scope =
      target.all_frames.value_or(false) ? ScriptExecutor::INCLUDE_SUB_FRAMES
                                        : ScriptExecutor::SPECIFIED_FRAMES;

  std::set<int> frame_ids;
  std::set<content::RenderFrameHost*> frames;
  if (!CollectFramesForInjection(target, web_contents, frame_ids, frames,
                                 error_out)) {
    return false;
  }

  // TODO(devlin): If `allFrames` is true, we error out if the extension
  // doesn't have access to the top frame (even if it may inject in child
  // frames). This is inconsistent with content scripts (which can execute
  // on child frames), but consistent with the old tabs.executeScript() API.
  for (content::RenderFrameHost* frame : frames) {
    DCHECK_EQ(content::WebContents::FromRenderFrameHost(frame), web_contents);
    if (!HasPermissionToInjectIntoFrame(permissions, target.tab_id, frame,
                                        error_out)) {
      return false;
    }
  }

  *frame_ids_out = std::move(frame_ids);
  *frame_scope_out = frame_scope;
  *script_executor_out = script_executor;
  return true;
}

void ExecuteScript(const ExtensionId& extension_id,
                   std::vector<mojom::JSSourcePtr> sources,
                   mojom::ExecutionWorld execution_world,
                   ScriptExecutor* script_executor,
                   ScriptExecutor::FrameScope frame_scope,
                   std::set<int> frame_ids,
                   bool inject_immediately,
                   bool user_gesture,
                   ScriptExecutor::ScriptFinishedCallback callback) {
  // Extensions can specify that the script should be injected "immediately".
  // In this case, we specify kDocumentStart as the injection time. Due to
  // inherent raciness between tab creation and load and this function
  // execution, there is no guarantee that it will actually happen at
  // document start, but the renderer will appropriately inject it
  // immediately if document start has already passed.
  mojom::RunLocation run_location = inject_immediately
                                        ? mojom::RunLocation::kDocumentStart
                                        : mojom::RunLocation::kDocumentIdle;
  script_executor->ExecuteScript(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension_id),
      mojom::CodeInjection::NewJs(mojom::JSInjection::New(
          std::move(sources), execution_world, /*world_id=*/std::nullopt,
          blink::mojom::WantResultOption::kWantResult,
          user_gesture ? blink::mojom::UserActivationOption::kActivate
                       : blink::mojom::UserActivationOption::kDoNotActivate,
          blink::mojom::PromiseResultOption::kAwait)),
      frame_scope, frame_ids, mojom::MatchOriginAsFallbackBehavior::kAlways,
      run_location, ScriptExecutor::DEFAULT_PROCESS,
      /*webview_src=*/GURL(), std::move(callback));
}

}  // namespace extensions::scripting
