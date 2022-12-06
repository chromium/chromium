// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_user_script_loader.h"

#include <stddef.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase_set.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/types/optional_util.h"
#include "base/version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/extension_types_utils.h"
#include "extensions/browser/api/scripting/scripting_constants.h"
#include "extensions/browser/api/scripting/scripting_utils.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/l10n_file_util.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/utils/content_script_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserContext;

namespace extensions {

namespace {

using SubstitutionMap = std::map<std::string, std::string>;

// Each map entry associates a UserScript::File object with the ID of the
// resource holding the content of the script.
using ScriptResourceIds = std::map<UserScript::File*, absl::optional<int>>;

// The source of script file from where it's read.
enum class ReadScriptContentSource {
  // ExtensionResource.
  kFile,
  // ResourceBundle.
  kResouceBundle,
};

struct VerifyContentInfo {
  VerifyContentInfo(const scoped_refptr<ContentVerifier>& verifier,
                    const ExtensionId& extension_id,
                    const base::FilePath& extension_root,
                    const base::FilePath relative_path,
                    absl::optional<std::string> content)
      : verifier(verifier),
        extension_id(extension_id),
        extension_root(extension_root),
        relative_path(relative_path),
        content(std::move(content)) {}

  // We explicitly disallow copying this because the `content` string may
  // be quite large for different extension files.
  VerifyContentInfo(const VerifyContentInfo&) = delete;
  VerifyContentInfo& operator=(VerifyContentInfo&) = delete;

  VerifyContentInfo(VerifyContentInfo&& other) = default;
  VerifyContentInfo& operator=(VerifyContentInfo&& other) = default;

  scoped_refptr<ContentVerifier> verifier;
  ExtensionId extension_id;
  base::FilePath extension_root;
  base::FilePath relative_path;

  // The content to verify, or nullopt if there was an error retrieving it
  // from its associated file. Example of errors are: missing or unreadable
  // file.
  absl::optional<std::string> content;
};

// Reads and returns {content, source} of a |script_file|.
//   - content contains the std::string content, or nullopt if the script file
// couldn't be read.
std::tuple<absl::optional<std::string>, ReadScriptContentSource>
ReadScriptContent(UserScript::File* script_file,
                  const absl::optional<int>& script_resource_id) {
  const base::FilePath& path = ExtensionResource::GetFilePath(
      script_file->extension_root(), script_file->relative_path(),
      ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT);
  if (path.empty()) {
    if (script_resource_id) {
      const ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      return {rb.LoadDataResourceString(*script_resource_id),
              ReadScriptContentSource::kResouceBundle};
    }
    LOG(WARNING) << "Failed to get file path to "
                 << script_file->relative_path().value() << " from "
                 << script_file->extension_root().value();
    return {absl::nullopt, ReadScriptContentSource::kFile};
  }

  std::string content;
  if (!base::ReadFileToString(path, &content)) {
    LOG(WARNING) << "Failed to load user script file: " << path.value();
    return {absl::nullopt, ReadScriptContentSource::kFile};
  }
  return {std::move(content), ReadScriptContentSource::kFile};
}

// Verifies file contents as they are read.
void VerifyContent(VerifyContentInfo info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(info.verifier);
  scoped_refptr<ContentVerifyJob> job(info.verifier->CreateAndStartJobFor(
      info.extension_id, info.extension_root, info.relative_path));
  if (job.get()) {
    if (info.content)
      job->Read(info.content->data(), info.content->size(), MOJO_RESULT_OK);
    else
      job->Read("", 0u, MOJO_RESULT_NOT_FOUND);
    job->Done();
  }
}

void ForwardVerifyContentToIO(VerifyContentInfo info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&VerifyContent, std::move(info)));
}

void RecordContentScriptLength(const std::string& script_content) {
  // Max bucket at 10 GB, which is way above the reasonable maximum size of a
  // script.
  static constexpr int kMaxUmaLengthInKB = 1024 * 1024 * 10;
  static constexpr int kMinUmaLengthInKB = 1;
  static constexpr int kBucketCount = 50;

  size_t content_script_length_kb = script_content.length() / 1024;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.ContentScripts.ContentScriptLength",
                              content_script_length_kb, kMinUmaLengthInKB,
                              kMaxUmaLengthInKB, kBucketCount);
}

// Records the total size in kb of all manifest and dynamic scripts that were
// loaded in a single load.
void RecordTotalContentScriptLengthForLoad(size_t manifest_scripts_length,
                                           size_t dynamic_scripts_length) {
  // Max bucket at 10 GB, which is way above the reasonable maximum size of all
  // scripts from a single extension.
  static constexpr int kMaxUmaLengthInKB = 1024 * 1024 * 10;
  static constexpr int kMinUmaLengthInKB = 1;
  static constexpr int kBucketCount = 50;

  // Only record a UMA entry if scripts were actually loaded, by checking if
  // the total scripts length is positive.
  if (manifest_scripts_length > 0u) {
    size_t manifest_scripts_length_kb = manifest_scripts_length / 1024;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Extensions.ContentScripts.ManifestContentScriptsLengthPerLoad",
        manifest_scripts_length_kb, kMinUmaLengthInKB, kMaxUmaLengthInKB,
        kBucketCount);
  }
  if (dynamic_scripts_length > 0u) {
    size_t dynamic_scripts_length_kb = dynamic_scripts_length / 1024;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Extensions.ContentScripts.DynamicContentScriptsLengthPerLoad",
        dynamic_scripts_length_kb, kMinUmaLengthInKB, kMaxUmaLengthInKB,
        kBucketCount);
  }
}

// Loads user scripts from the extension who owns these scripts.
void LoadScriptContent(const mojom::HostID& host_id,
                       UserScript::File* script_file,
                       const absl::optional<int>& script_resource_id,
                       const SubstitutionMap* localization_messages,
                       const scoped_refptr<ContentVerifier>& verifier) {
  DCHECK(script_file);
  auto [content, source] = ReadScriptContent(script_file, script_resource_id);

  bool needs_content_verification = source == ReadScriptContentSource::kFile;
  if (needs_content_verification && verifier.get()) {
    // Note: |content| is nullopt here for missing / unreadable file. We still
    // pass it through ContentVerifier to report content verification error.
    VerifyContentInfo info(verifier, host_id.id, script_file->extension_root(),
                           script_file->relative_path(), content);

    // Call VerifyContent() after yielding on UI thread so it is ensured that
    // ContentVerifierIOData is populated at the time we call VerifyContent().
    // Priority set explicitly to avoid unwanted task priority inheritance.
    content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
        ->PostTask(FROM_HERE,
                   base::BindOnce(&ForwardVerifyContentToIO, std::move(info)));
  }

  if (!content)
    return;

  // Localize the content.
  if (localization_messages) {
    std::string error;
    MessageBundle::ReplaceMessagesWithExternalDictionary(*localization_messages,
                                                         &*content, &error);
    if (!error.empty())
      LOG(WARNING) << "Failed to replace messages in script: " << error;
  }

  // Remove BOM from the content.
  if (base::StartsWith(*content, base::kUtf8ByteOrderMark,
                       base::CompareCase::SENSITIVE)) {
    std::string trimmed_content =
        content->substr(strlen(base::kUtf8ByteOrderMark));
    RecordContentScriptLength(trimmed_content);
    script_file->set_content(trimmed_content);
  } else {
    RecordContentScriptLength(*content);
    script_file->set_content(*content);
  }
}

void FillScriptFileResourceIds(const UserScript::FileList& script_files,
                               ScriptResourceIds& script_resource_ids) {
  const ComponentExtensionResourceManager* extension_resource_manager =
      ExtensionsBrowserClient::Get()->GetComponentExtensionResourceManager();
  if (!extension_resource_manager)
    return;

  for (const std::unique_ptr<UserScript::File>& script_file : script_files) {
    if (!script_file->GetContent().empty())
      continue;
    int resource_id = 0;
    if (extension_resource_manager->IsComponentExtensionResource(
            script_file->extension_root(), script_file->relative_path(),
            &resource_id)) {
      script_resource_ids[script_file.get()] = resource_id;
    }
  }
}

void LoadUserScripts(
    UserScriptList* user_scripts,
    ScriptResourceIds script_resource_ids,
    const ExtensionUserScriptLoader::PathAndLocaleInfo& host_info,
    const std::set<std::string>& added_script_ids,
    const scoped_refptr<ContentVerifier>& verifier) {
  // Tracks the total size in bytes for `user_scripts` for this script load.
  // These counts are separate for manifest and dynamic scripts. All scripts in
  // `user_scripts` are from the same extension.
  size_t manifest_script_length = 0u;
  size_t dynamic_script_length = 0u;

  for (const std::unique_ptr<UserScript>& script : *user_scripts) {
    size_t script_files_length = 0u;

    if (added_script_ids.count(script->id()) == 0)
      continue;
    for (const std::unique_ptr<UserScript::File>& script_file :
         script->js_scripts()) {
      if (script_file->GetContent().empty()) {
        LoadScriptContent(script->host_id(), script_file.get(),
                          script_resource_ids[script_file.get()], nullptr,
                          verifier);
      }

      script_files_length += script_file->GetContent().length();
    }
    if (script->css_scripts().size() > 0) {
      std::unique_ptr<SubstitutionMap> localization_messages(
          l10n_file_util::LoadMessageBundleSubstitutionMap(
              host_info.file_path, script->host_id().id,
              host_info.default_locale, host_info.gzip_permission));

      for (const std::unique_ptr<UserScript::File>& script_file :
           script->css_scripts()) {
        if (script_file->GetContent().empty()) {
          LoadScriptContent(script->host_id(), script_file.get(),
                            script_resource_ids[script_file.get()],
                            localization_messages.get(), verifier);
        }

        script_files_length += script_file->GetContent().length();
      }
    }

    if (script->IsIDGenerated()) {
      manifest_script_length += script_files_length;
    } else {
      dynamic_script_length += script_files_length;
    }
  }

  RecordTotalContentScriptLengthForLoad(manifest_script_length,
                                        dynamic_script_length);
}

void LoadScriptsOnFileTaskRunner(
    std::unique_ptr<UserScriptList> user_scripts,
    ScriptResourceIds script_resource_ids,
    const ExtensionUserScriptLoader::PathAndLocaleInfo& host_info,
    const std::set<std::string>& added_script_ids,
    const scoped_refptr<ContentVerifier>& verifier,
    UserScriptLoader::LoadScriptsCallback callback) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(user_scripts.get());
  LoadUserScripts(user_scripts.get(), std::move(script_resource_ids), host_info,
                  added_script_ids, verifier);
  base::ReadOnlySharedMemoryRegion memory =
      UserScriptLoader::Serialize(*user_scripts);
  // Explicit priority to prevent unwanted task priority inheritance.
  content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(std::move(callback), std::move(user_scripts),
                                std::move(memory)));
}

UserScriptList ConvertValueToScripts(const Extension& extension,
                                     const base::Value::List& list) {
  const int valid_schemes = UserScript::ValidUserScriptSchemes(
      scripting::kScriptsCanExecuteEverywhere);

  UserScriptList scripts;
  for (const base::Value& value : list) {
    std::u16string error;
    std::unique_ptr<api::content_scripts::ContentScript> content_script =
        api::content_scripts::ContentScript::FromValue(value, &error);

    if (!content_script)
      continue;

    std::unique_ptr<UserScript> script = std::make_unique<UserScript>();
    const auto& dict = value.GetDict();
    auto* id = dict.FindString(scripting::kId);
    if (!id)
      continue;

    script->set_id(*id);
    script->set_host_id(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension.id()));

    if (content_script->all_frames)
      script->set_match_all_frames(*content_script->all_frames);
    script->set_run_location(
        script_parsing::ConvertManifestRunLocation(content_script->run_at));
    script->set_execution_world(ConvertExecutionWorld(content_script->world));

    if (!script_parsing::ParseMatchPatterns(
            content_script->matches,
            base::OptionalToPtr(content_script->exclude_matches),
            /*definition_index=*/0, extension.creation_flags(),
            scripting::kScriptsCanExecuteEverywhere, valid_schemes,
            scripting::kAllUrlsIncludesChromeUrls, script.get(), &error,
            /*wants_file_access=*/nullptr)) {
      continue;
    }

    if (!script_parsing::ParseFileSources(
            &extension, base::OptionalToPtr(content_script->js),
            base::OptionalToPtr(content_script->css),
            /*definition_index=*/0, script.get(), &error)) {
      continue;
    }

    scripts.push_back(std::move(script));
  }

  return scripts;
}

// TODO(crbug.com/1248314): Eventually use
// api::scripting::RegisteredContentScript instead as an intermediate type which
// then gets converted to base::Value.
api::content_scripts::ContentScript CreateContentScriptObject(
    const UserScript& script) {
  api::content_scripts::ContentScript content_script;

  content_script.matches.reserve(script.url_patterns().size());
  for (const URLPattern& pattern : script.url_patterns())
    content_script.matches.push_back(pattern.GetAsString());

  if (!script.exclude_url_patterns().is_empty()) {
    content_script.exclude_matches.emplace();
    content_script.exclude_matches->reserve(
        script.exclude_url_patterns().size());
    for (const URLPattern& pattern : script.exclude_url_patterns())
      content_script.exclude_matches->push_back(pattern.GetAsString());
  }

  // File paths may be normalized in the returned object and can differ slightly
  // compared to what was originally passed into registerContentScripts.
  if (!script.js_scripts().empty()) {
    content_script.js.emplace();
    content_script.js->reserve(script.js_scripts().size());
    for (const auto& js_script : script.js_scripts())
      content_script.js->push_back(js_script->relative_path().AsUTF8Unsafe());
  }

  if (!script.css_scripts().empty()) {
    content_script.css.emplace();
    content_script.css->reserve(script.css_scripts().size());
    for (const auto& css_script : script.css_scripts())
      content_script.css->push_back(css_script->relative_path().AsUTF8Unsafe());
  }

  content_script.all_frames = script.match_all_frames();
  content_script.match_origin_as_fallback =
      script.match_origin_as_fallback() ==
      MatchOriginAsFallbackBehavior::kAlways;

  content_script.run_at =
      script_parsing::ConvertRunLocationToManifestType(script.run_location());
  content_script.world = ConvertExecutionWorldForAPI(script.execution_world());
  return content_script;
}

// Gets an extension's manifest scripts' metadata; i.e., gets a list of
// UserScript objects that contains script info, but not the contents of the
// scripts.
std::unique_ptr<UserScriptList> GetManifestScriptsMetadata(
    content::BrowserContext* browser_context,
    const Extension& extension) {
  bool incognito_enabled =
      util::IsIncognitoEnabled(extension.id(), browser_context);
  const UserScriptList& script_list =
      ContentScriptsInfo::GetContentScripts(&extension);
  auto script_vector = std::make_unique<UserScriptList>();
  script_vector->reserve(script_list.size());
  for (const auto& script : script_list) {
    std::unique_ptr<UserScript> script_copy =
        UserScript::CopyMetadataFrom(*script);
    script_copy->set_incognito_enabled(incognito_enabled);
    script_vector->push_back(std::move(script_copy));
  }
  return script_vector;
}

}  // namespace

ExtensionUserScriptLoader::ExtensionUserScriptLoader(
    BrowserContext* browser_context,
    const Extension& extension,
    StateStore* state_store,
    bool listen_for_extension_system_loaded)
    : ExtensionUserScriptLoader(
          browser_context,
          extension,
          state_store,
          listen_for_extension_system_loaded,
          ExtensionSystem::Get(browser_context)->content_verifier()) {}

ExtensionUserScriptLoader::ExtensionUserScriptLoader(
    BrowserContext* browser_context,
    const Extension& extension,
    StateStore* state_store,
    bool listen_for_extension_system_loaded,
    scoped_refptr<ContentVerifier> content_verifier)
    : UserScriptLoader(
          browser_context,
          mojom::HostID(mojom::HostID::HostType::kExtensions, extension.id())),
      host_info_({extension.path(), LocaleInfo::GetDefaultLocale(&extension),
                  extension_l10n_util::GetGzippedMessagesPermissionForExtension(
                      &extension)}),
      helper_(browser_context, extension.id(), state_store),
      content_verifier_(std::move(content_verifier)) {
  if (listen_for_extension_system_loaded) {
    ExtensionSystem::Get(browser_context)
        ->ready()
        .Post(FROM_HERE,
              base::BindOnce(&ExtensionUserScriptLoader::OnExtensionSystemReady,
                             weak_factory_.GetWeakPtr()));
  } else {
    SetReady(true);
  }
}

ExtensionUserScriptLoader::~ExtensionUserScriptLoader() {}

void ExtensionUserScriptLoader::AddPendingDynamicScriptIDs(
    std::set<std::string> script_ids) {
  pending_dynamic_script_ids_.insert(
      std::make_move_iterator(script_ids.begin()),
      std::make_move_iterator(script_ids.end()));
}

void ExtensionUserScriptLoader::RemovePendingDynamicScriptIDs(
    const std::set<std::string>& script_ids) {
  for (const auto& id : script_ids)
    pending_dynamic_script_ids_.erase(id);
}

bool ExtensionUserScriptLoader::AddScriptsForExtensionLoad(
    const Extension& extension,
    UserScriptLoader::ScriptsLoadedCallback callback) {
  std::unique_ptr<UserScriptList> manifest_scripts =
      GetManifestScriptsMetadata(browser_context(), extension);
  bool has_dynamic_scripts = HasInitialDynamicScripts(extension);

  if (manifest_scripts->empty() && !has_dynamic_scripts)
    return false;

  if (has_dynamic_scripts) {
    helper_.GetDynamicScripts(base::BindOnce(
        &ExtensionUserScriptLoader::OnInitialDynamicScriptsReadFromStateStore,
        weak_factory_.GetWeakPtr(), std::move(manifest_scripts),
        std::move(callback)));
  } else {
    AddScripts(std::move(manifest_scripts), std::move(callback));
  }

  return true;
}

void ExtensionUserScriptLoader::AddDynamicScripts(
    std::unique_ptr<UserScriptList> scripts,
    std::set<std::string> persistent_script_ids,
    DynamicScriptsModifiedCallback callback) {
  auto scripts_metadata = std::make_unique<UserScriptList>();
  for (const std::unique_ptr<UserScript>& script : *scripts) {
    // Only proceed with adding scripts that the extension still intends to add.
    // This guards again an edge case where scripts registered by an API call
    // are quickly unregistered.
    if (base::Contains(pending_dynamic_script_ids_, script->id()))
      scripts_metadata->push_back(UserScript::CopyMetadataFrom(*script));
  }

  if (scripts_metadata->empty()) {
    std::move(callback).Run(/*error=*/absl::nullopt);
    return;
  }

  AddScripts(
      std::move(scripts),
      base::BindOnce(&ExtensionUserScriptLoader::OnDynamicScriptsAdded,
                     weak_factory_.GetWeakPtr(), std::move(scripts_metadata),
                     std::move(persistent_script_ids), std::move(callback)));
}

void ExtensionUserScriptLoader::RemoveDynamicScripts(
    const std::set<std::string>& ids_to_remove,
    DynamicScriptsModifiedCallback callback) {
  if (ids_to_remove.empty()) {
    std::move(callback).Run(/*error=*/absl::nullopt);
    return;
  }

  // Remove pending script ids first, so loads from previous operations which
  // complete later will recognize the change.
  RemovePendingDynamicScriptIDs(ids_to_remove);
  RemoveScripts(
      ids_to_remove,
      base::BindOnce(&ExtensionUserScriptLoader::OnDynamicScriptsRemoved,
                     weak_factory_.GetWeakPtr(), ids_to_remove,
                     std::move(callback)));
}

void ExtensionUserScriptLoader::ClearDynamicScripts(
    DynamicScriptsModifiedCallback callback) {
  RemoveDynamicScripts(GetDynamicScriptIDs(), std::move(callback));
}

std::set<std::string> ExtensionUserScriptLoader::GetDynamicScriptIDs() const {
  std::set<std::string> dynamic_script_ids;
  dynamic_script_ids.insert(pending_dynamic_script_ids_.begin(),
                            pending_dynamic_script_ids_.end());

  for (const std::unique_ptr<UserScript>& script : loaded_dynamic_scripts_)
    dynamic_script_ids.insert(script->id());

  return dynamic_script_ids;
}

const UserScriptList& ExtensionUserScriptLoader::GetLoadedDynamicScripts()
    const {
  return loaded_dynamic_scripts_;
}

std::set<std::string> ExtensionUserScriptLoader::GetPersistentDynamicScriptIDs()
    const {
  return persistent_dynamic_script_ids_;
}

std::unique_ptr<UserScriptList> ExtensionUserScriptLoader::LoadScriptsForTest(
    std::unique_ptr<UserScriptList> user_scripts) {
  std::set<std::string> added_script_ids;
  for (const std::unique_ptr<UserScript>& script : *user_scripts)
    added_script_ids.insert(script->id());

  std::unique_ptr<UserScriptList> result;

  // Block until the scripts have been loaded on the file task runner so that
  // we can return the result synchronously.
  base::RunLoop run_loop;
  LoadScripts(std::move(user_scripts), added_script_ids,
              base::BindOnce(
                  [](base::OnceClosure done_callback,
                     std::unique_ptr<UserScriptList>& loaded_user_scripts,
                     std::unique_ptr<UserScriptList> user_scripts,
                     base::ReadOnlySharedMemoryRegion /* shared_memory */) {
                    loaded_user_scripts = std::move(user_scripts);
                    std::move(done_callback).Run();
                  },
                  run_loop.QuitClosure(), std::ref(result)));
  run_loop.Run();

  return result;
}

ExtensionUserScriptLoader::DynamicScriptsStorageHelper::
    DynamicScriptsStorageHelper(BrowserContext* browser_context,
                                const ExtensionId& extension_id,
                                StateStore* state_store)
    : browser_context_(browser_context),
      extension_id_(extension_id),
      state_store_(state_store) {}

ExtensionUserScriptLoader::DynamicScriptsStorageHelper::
    ~DynamicScriptsStorageHelper() = default;

void ExtensionUserScriptLoader::DynamicScriptsStorageHelper::GetDynamicScripts(
    DynamicScriptsReadCallback callback) {
  if (!state_store_) {
    std::move(callback).Run(UserScriptList());
    return;
  }

  state_store_->GetExtensionValue(
      extension_id_, scripting::kRegisteredScriptsStorageKey,
      base::BindOnce(&ExtensionUserScriptLoader::DynamicScriptsStorageHelper::
                         OnDynamicScriptsReadFromStorage,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionUserScriptLoader::DynamicScriptsStorageHelper::SetDynamicScripts(
    const UserScriptList& scripts,
    const std::set<std::string>& persistent_dynamic_script_ids) {
  if (!state_store_)
    return;

  base::Value::List scripts_value;
  URLPatternSet persistent_patterns;
  for (const std::unique_ptr<UserScript>& script : scripts) {
    if (!base::Contains(persistent_dynamic_script_ids, script->id()))
      continue;

    base::Value::Dict value = CreateContentScriptObject(*script).ToValue();
    value.Set(scripting::kId, script->id());

    scripts_value.Append(std::move(value));
    persistent_patterns.AddPatterns(script->url_patterns());
  }

  scripting::SetPersistentScriptURLPatterns(browser_context_, extension_id_,
                                            std::move(persistent_patterns));
  state_store_->SetExtensionValue(extension_id_,
                                  scripting::kRegisteredScriptsStorageKey,
                                  base::Value(std::move(scripts_value)));
}

void ExtensionUserScriptLoader::DynamicScriptsStorageHelper::
    OnDynamicScriptsReadFromStorage(DynamicScriptsReadCallback callback,
                                    absl::optional<base::Value> value) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id_);
  DCHECK(extension) << "The ExtensionUserScriptLoader should have been cleaned "
                       "up if the extension was disabled";

  UserScriptList scripts;
  if (value && value->is_list()) {
    UserScriptList dynamic_scripts =
        ConvertValueToScripts(*extension, value->GetList());
    scripts.insert(scripts.end(),
                   std::make_move_iterator(dynamic_scripts.begin()),
                   std::make_move_iterator(dynamic_scripts.end()));
  }

  std::move(callback).Run(std::move(scripts));
}

void ExtensionUserScriptLoader::LoadScripts(
    std::unique_ptr<UserScriptList> user_scripts,
    const std::set<std::string>& added_script_ids,
    LoadScriptsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScriptResourceIds script_resource_ids;
  for (const std::unique_ptr<UserScript>& script : *user_scripts) {
    if (!base::Contains(added_script_ids, script->id()))
      continue;
    FillScriptFileResourceIds(script->js_scripts(), script_resource_ids);
    FillScriptFileResourceIds(script->css_scripts(), script_resource_ids);
  }

  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&LoadScriptsOnFileTaskRunner, std::move(user_scripts),
                     std::move(script_resource_ids), host_info_,
                     added_script_ids, content_verifier_, std::move(callback)));
}

void ExtensionUserScriptLoader::OnExtensionSystemReady() {
  SetReady(true);
}

void ExtensionUserScriptLoader::OnInitialDynamicScriptsReadFromStateStore(
    std::unique_ptr<UserScriptList> scripts,
    UserScriptLoader::ScriptsLoadedCallback callback,
    UserScriptList initial_dynamic_scripts) {
  auto dynamic_scripts_metadata = std::make_unique<UserScriptList>();
  for (const std::unique_ptr<UserScript>& script : initial_dynamic_scripts) {
    dynamic_scripts_metadata->push_back(UserScript::CopyMetadataFrom(*script));
    pending_dynamic_script_ids_.insert(script->id());
  }

  scripts->insert(scripts->end(),
                  std::make_move_iterator(initial_dynamic_scripts.begin()),
                  std::make_move_iterator(initial_dynamic_scripts.end()));

  AddScripts(std::move(scripts),
             base::BindOnce(
                 &ExtensionUserScriptLoader::OnInitialExtensionScriptsLoaded,
                 weak_factory_.GetWeakPtr(),
                 std::move(dynamic_scripts_metadata), std::move(callback)));
}

void ExtensionUserScriptLoader::OnInitialExtensionScriptsLoaded(
    std::unique_ptr<UserScriptList> initial_dynamic_scripts,
    UserScriptLoader::ScriptsLoadedCallback callback,
    UserScriptLoader* loader,
    const absl::optional<std::string>& error) {
  for (const std::unique_ptr<UserScript>& script : *initial_dynamic_scripts)
    pending_dynamic_script_ids_.erase(script->id());

  if (!error.has_value()) {
    for (const std::unique_ptr<UserScript>& script : *initial_dynamic_scripts)
      persistent_dynamic_script_ids_.insert(script->id());
    loaded_dynamic_scripts_.insert(
        loaded_dynamic_scripts_.end(),
        std::make_move_iterator(initial_dynamic_scripts->begin()),
        std::make_move_iterator(initial_dynamic_scripts->end()));
  }

  std::move(callback).Run(loader, error);
}

void ExtensionUserScriptLoader::OnDynamicScriptsAdded(
    std::unique_ptr<UserScriptList> added_scripts,
    std::set<std::string> new_persistent_script_ids,
    DynamicScriptsModifiedCallback callback,
    UserScriptLoader* loader,
    const absl::optional<std::string>& error) {
  // Now that a script load for all scripts contained in `added_scripts` has
  // occurred, add these scripts to `loaded_dynamic_scripts_` and remove any ids
  // in `pending_dynamic_script_ids_` that correspond to a script in
  // `added_scripts`.
  for (const std::unique_ptr<UserScript>& script : *added_scripts)
    pending_dynamic_script_ids_.erase(script->id());

  if (!error.has_value()) {
    loaded_dynamic_scripts_.insert(
        loaded_dynamic_scripts_.end(),
        std::make_move_iterator(added_scripts->begin()),
        std::make_move_iterator(added_scripts->end()));

    persistent_dynamic_script_ids_.insert(
        std::make_move_iterator(new_persistent_script_ids.begin()),
        std::make_move_iterator(new_persistent_script_ids.end()));

    helper_.SetDynamicScripts(loaded_dynamic_scripts_,
                              persistent_dynamic_script_ids_);
  }

  std::move(callback).Run(error);
}

void ExtensionUserScriptLoader::OnDynamicScriptsRemoved(
    const std::set<std::string>& removed_script_ids,
    DynamicScriptsModifiedCallback callback,
    UserScriptLoader* loader,
    const absl::optional<std::string>& error) {
  // Remove scripts from `loaded_dynamic_scripts_` only when the set of
  // `removed_script_ids` have actually been removed and the corresponding IPC
  // has been sent.
  if (!error.has_value()) {
    base::EraseIf(
        loaded_dynamic_scripts_,
        [&removed_script_ids](const std::unique_ptr<UserScript>& script) {
          return base::Contains(removed_script_ids, script->id());
        });

    base::EraseIf(persistent_dynamic_script_ids_,
                  [&removed_script_ids](const auto& id) {
                    return base::Contains(removed_script_ids, id);
                  });

    helper_.SetDynamicScripts(loaded_dynamic_scripts_,
                              persistent_dynamic_script_ids_);
  }

  std::move(callback).Run(error);
}

bool ExtensionUserScriptLoader::HasInitialDynamicScripts(
    const Extension& extension) const {
  bool has_scripting_permission =
      extension.permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kScripting);
  if (!has_scripting_permission)
    return false;

  URLPatternSet initial_dynamic_patterns =
      scripting::GetPersistentScriptURLPatterns(browser_context(),
                                                extension.id());
  return !initial_dynamic_patterns.is_empty();
}

}  // namespace extensions
