// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_user_script_loader.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/l10n_file_util.h"
#include "extensions/browser/scripting_constants.h"
#include "extensions/browser/scripting_utils.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/api/scripts_internal.h"
#include "extensions/common/api/scripts_internal/script_serialization.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"
#include "extensions/common/utils/content_script_utils.h"
#include "extensions/common/utils/extension_types_utils.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserContext;

namespace extensions {

namespace {

using SubstitutionMap = std::map<std::string, std::string>;

// Each map entry associates a UserScript::Content object with the ID of the
// resource holding the content of the script.
using ScriptResourceIds = std::map<UserScript::Content*, std::optional<int>>;

// The source of script file from where it's read.
enum class ReadScriptContentSource {
  // ExtensionResource.
  kFile,
  // ResourceBundle.
  kResourceBundle,
};

// The key for storing a dynamic content script's id.
inline constexpr char kId[] = "id";

// Reads and returns {content, source} of a |script_file|.
//   - content contains the std::string content, or nullopt if the script file
// couldn't be read.
std::tuple<std::optional<std::string>, ReadScriptContentSource>
ReadScriptContent(UserScript::Content* script_file,
                  const std::optional<int>& script_resource_id,
                  size_t& remaining_length) {
  const base::FilePath& path = ExtensionResource::GetFilePath(
      script_file->extension_root(), script_file->relative_path(),
      ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT);
  if (path.empty()) {
    if (script_resource_id) {
      const ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      return {rb.LoadDataResourceString(*script_resource_id),
              ReadScriptContentSource::kResourceBundle};
    }
    LOG(WARNING) << "Failed to get file path to "
                 << script_file->relative_path().value() << " from "
                 << script_file->extension_root().value();
    return {std::nullopt, ReadScriptContentSource::kFile};
  }

  size_t max_script_length =
      std::min(remaining_length, script_parsing::GetMaxScriptLength());
  std::string content;
  if (!base::ReadFileToStringWithMaxSize(path, &content, max_script_length)) {
    if (content.empty()) {
      LOG(WARNING) << "Failed to load user script file: " << path.value();
    } else {
      LOG(WARNING) << "Failed to load user script file, maximum size exceeded: "
                   << path.value();
    }
    return {std::nullopt, ReadScriptContentSource::kFile};
  }

  remaining_length -= content.size();
  return {std::move(content), ReadScriptContentSource::kFile};
}

// Verifies file contents as they are read.
void VerifyContent(ContentVerifier* verifier,
                   const ExtensionId& extension_id,
                   const base::FilePath& extension_root,
                   const base::FilePath& relative_path,
                   const std::optional<std::string>& content) {
  DCHECK(verifier);
  scoped_refptr<ContentVerifyJob> job(ContentVerifier::CreateAndStartJobFor(
      extension_id, extension_root, relative_path, verifier));
  CHECK(job);
  if (content) {
    job->BytesRead(content->data(), content->size(), MOJO_RESULT_OK);
  } else {
    job->BytesRead("", 0u, MOJO_RESULT_NOT_FOUND);
  }
  job->DoneReading();
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
                       UserScript::Content* script_file,
                       const std::optional<int>& script_resource_id,
                       const SubstitutionMap* localization_messages,
                       const scoped_refptr<ContentVerifier>& verifier,
                       size_t& remaining_length) {
  DCHECK(script_file);
  auto [content, source] =
      ReadScriptContent(script_file, script_resource_id, remaining_length);

  bool needs_content_verification = source == ReadScriptContentSource::kFile;
  if (needs_content_verification && verifier.get()) {
    // Note: |content| is nullopt here for missing / unreadable file. We still
    // pass it through ContentVerifier to report content verification error.
    VerifyContent(verifier.get(), host_id.id, script_file->extension_root(),
                  script_file->relative_path(), content);
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
    content->erase(0, strlen(base::kUtf8ByteOrderMark));
  }
  RecordContentScriptLength(*content);
  script_file->set_content(std::move(*content));
}

void FillScriptFileResourceIds(const UserScript::ContentList& script_files,
                               ScriptResourceIds& script_resource_ids) {
  const ComponentExtensionResourceManager* extension_resource_manager =
      ExtensionsBrowserClient::Get()->GetComponentExtensionResourceManager();
  if (!extension_resource_manager)
    return;

  for (const std::unique_ptr<UserScript::Content>& script_file : script_files) {
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

// Returns the total length of scripts that were previously loaded (i.e. not
// present in `added_script_ids`).
size_t GetTotalLoadedScriptsLength(
    UserScriptList* user_scripts,
    const std::set<std::string>& added_script_ids) {
  size_t total_length = 0u;
  for (const std::unique_ptr<UserScript>& script : *user_scripts) {
    if (added_script_ids.count(script->id()) == 0) {
      for (const auto& js_script : script->js_scripts())
        total_length += js_script->GetContent().length();
      for (const auto& js_script : script->css_scripts())
        total_length += js_script->GetContent().length();
    }
  }
  return total_length;
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

  // Calculate the remaining storage allocated for scripts for this extension by
  // subtracting the length of all loaded scripts from the extension's max
  // scripts length. Note that subtraction is only done if the result will be
  // positive (to avoid unsigned wraparound).
  size_t loaded_length =
      GetTotalLoadedScriptsLength(user_scripts, added_script_ids);
  size_t remaining_length =
      loaded_length >= script_parsing::GetMaxScriptsLengthPerExtension()
          ? 0u
          : script_parsing::GetMaxScriptsLengthPerExtension() - loaded_length;

  for (const std::unique_ptr<UserScript>& script : *user_scripts) {
    size_t script_files_length = 0u;

    if (added_script_ids.count(script->id()) == 0)
      continue;
    for (const std::unique_ptr<UserScript::Content>& script_file :
         script->js_scripts()) {
      if (script_file->GetContent().empty()) {
        LoadScriptContent(script->host_id(), script_file.get(),
                          script_resource_ids[script_file.get()], nullptr,
                          verifier, remaining_length);
      }

      script_files_length += script_file->GetContent().length();
    }
    if (script->css_scripts().size() > 0) {
      std::unique_ptr<SubstitutionMap> localization_messages =
          l10n_file_util::LoadMessageBundleSubstitutionMap(
              host_info.file_path, script->host_id().id,
              host_info.default_locale, host_info.gzip_permission);

      for (const std::unique_ptr<UserScript::Content>& script_file :
           script->css_scripts()) {
        if (script_file->GetContent().empty()) {
          LoadScriptContent(script->host_id(), script_file.get(),
                            script_resource_ids[script_file.get()],
                            localization_messages.get(), verifier,
                            remaining_length);
        }

        script_files_length += script_file->GetContent().length();
      }
    }

    switch (script->GetSource()) {
      case UserScript::Source::kStaticContentScript:
        manifest_script_length += script_files_length;
        break;
      case UserScript::Source::kDynamicContentScript:
      case UserScript::Source::kDynamicUserScript:
        dynamic_script_length += script_files_length;
        break;
      case UserScript::Source::kWebUIScript:
        NOTREACHED_IN_MIGRATION();
    }
  }

  RecordTotalContentScriptLengthForLoad(manifest_script_length,
                                        dynamic_script_length);
}

void LoadScriptsOnFileTaskRunner(
    UserScriptList user_scripts,
    ScriptResourceIds script_resource_ids,
    const ExtensionUserScriptLoader::PathAndLocaleInfo& host_info,
    const std::set<std::string>& added_script_ids,
    const scoped_refptr<ContentVerifier>& verifier,
    UserScriptLoader::LoadScriptsCallback callback) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  LoadUserScripts(&user_scripts, std::move(script_resource_ids), host_info,
                  added_script_ids, verifier);
  base::ReadOnlySharedMemoryRegion memory =
      UserScriptLoader::Serialize(user_scripts);
  // Explicit priority to prevent unwanted task priority inheritance.
  content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(std::move(callback), std::move(user_scripts),
                                std::move(memory)));
}

// Attempts to coerce a `dict` from an `api::content_scripts::ContentScript` to
// an `api::scripts_internal::SerializedUserScript`, returning std::nullopt on
// failure.
// TODO(crbug.com/40286091): Remove this when migration is complete.
std::optional<api::scripts_internal::SerializedUserScript>
ContentScriptDictToSerializedUserScript(const base::Value::Dict& dict) {
  auto content_script = api::content_scripts::ContentScript::FromValue(dict);
  if (!content_script.has_value()) {
    return std::nullopt;  // Bad entry.
  }

  auto* id = dict.FindString(kId);
  if (!id || id->empty()) {
    return std::nullopt;  // Bad entry.
  }

  // If a UserScript does not have a prefixed ID, then we can assume it's a
  // dynamic content script, as was historically the case.
  std::string id_to_use;
  api::scripts_internal::Source source =
      api::scripts_internal::Source::kDynamicContentScript;
  bool is_id_prefixed = (*id)[0] == UserScript::kReservedScriptIDPrefix;
  if (is_id_prefixed) {
    // Note: We don't use UserScript::GetSourceForScriptID() since:
    // - That method allows for static content scripts, which aren't stored
    //   here, and
    // - That method requires input to be valid (crashing otherwise), and we
    //   have no guarantee of that here.
    if (base::StartsWith(*id, UserScript::kDynamicContentScriptPrefix)) {
      source = api::scripts_internal::Source::kDynamicContentScript;
    } else if (base::StartsWith(*id, UserScript::kDynamicUserScriptPrefix)) {
      source = api::scripts_internal::Source::kDynamicUserScript;
    } else {
      // Invalid script source. Bad entry.
      return std::nullopt;
    }
    id_to_use = *id;
  } else {
    id_to_use = scripting::AddPrefixToDynamicScriptId(
        *id, UserScript::Source::kDynamicContentScript);
    source = api::scripts_internal::Source::kDynamicContentScript;
  }

  // At this point, the entry is considered valid, and we just convert it over
  // to the serialized type.

  auto file_strings_to_script_sources = [](std::vector<std::string> files) {
    std::vector<api::scripts_internal::ScriptSource> sources;
    sources.reserve(files.size());
    for (auto& file : files) {
      api::scripts_internal::ScriptSource source;
      source.file = std::move(file);
      sources.push_back(std::move(source));
    }
    return sources;
  };

  api::scripts_internal::SerializedUserScript serialized_script;
  serialized_script.all_frames = content_script->all_frames;
  if (content_script->css) {
    serialized_script.css =
        file_strings_to_script_sources(std::move(*content_script->css));
  }
  serialized_script.exclude_matches =
      std::move(content_script->exclude_matches);
  serialized_script.exclude_globs = std::move(content_script->exclude_globs);
  serialized_script.id = std::move(id_to_use);
  serialized_script.include_globs = std::move(content_script->include_globs);
  if (content_script->js) {
    serialized_script.js =
        file_strings_to_script_sources(std::move(*content_script->js));
  }
  serialized_script.matches = std::move(content_script->matches);
  serialized_script.match_origin_as_fallback =
      content_script->match_origin_as_fallback;
  serialized_script.run_at = content_script->run_at;
  serialized_script.source = source;
  serialized_script.world = content_script->world;

  return serialized_script;
}

// Converts the list of values in `list` to a UserScriptList.
UserScriptList ConvertValueToScripts(const Extension& extension,
                                     bool allowed_in_incognito,
                                     const base::Value::List& list) {
  UserScriptList scripts;
  for (const base::Value& value : list) {
    if (!value.is_dict()) {
      continue;  // Bad entry; no recovery.
    }

    std::optional<api::scripts_internal::SerializedUserScript>
        serialized_script;

    // Check for the `source` key as a sentinel to determine if the underlying
    // type is the old one we used, api::content_scripts::ContentScript, or is
    // the new api::scripts_internal::SerializedUserScript. The `source` key is
    // only present on the new type.
    if (!value.GetDict().Find("source")) {
      // It's the old type, or could be a bad entry.

      // TODO(crbug.com/40286091): Add UMA and forced-migration so we
      // can remove this code.
      serialized_script =
          ContentScriptDictToSerializedUserScript(value.GetDict());
    } else {
      serialized_script =
          api::scripts_internal::SerializedUserScript::FromValue(value);
    }

    if (!serialized_script || serialized_script->id.empty()) {
      continue;  // Bad entry.
    }

    std::unique_ptr<UserScript> parsed_script =
        script_serialization::ParseSerializedUserScript(
            *serialized_script, extension, allowed_in_incognito);

    if (!parsed_script) {
      continue;  // Bad entry.
    }

    scripts.push_back(std::move(parsed_script));
  }

  return scripts;
}

// Gets an extension's manifest scripts' metadata; i.e., gets a list of
// UserScript objects that contains script info, but not the contents of the
// scripts.
UserScriptList GetManifestScriptsMetadata(
    content::BrowserContext* browser_context,
    const Extension& extension) {
  bool incognito_enabled =
      util::IsIncognitoEnabled(extension.id(), browser_context);
  const UserScriptList& script_list =
      ContentScriptsInfo::GetContentScripts(&extension);
  UserScriptList script_vector;
  script_vector.reserve(script_list.size());
  for (const auto& script : script_list) {
    std::unique_ptr<UserScript> script_copy =
        UserScript::CopyMetadataFrom(*script);
    script_copy->set_incognito_enabled(incognito_enabled);
    script_vector.push_back(std::move(script_copy));
  }
  return script_vector;
}

// Returns a copy of the dynamic `script` info, which includes the script
// content when its source is inline code.
std::unique_ptr<UserScript> CopyDynamicScriptInfo(const UserScript& script) {
  std::unique_ptr<UserScript> script_metadata =
      UserScript::CopyMetadataFrom(script);

  // When the script source is inline code, we need to add the content of the
  // script to the script metadata so it can be properly persisted/retrieved.
  for (size_t i = 0; i < script_metadata->js_scripts().size(); i++) {
    auto& content = script_metadata->js_scripts().at(i);
    if (content->source() == UserScript::Content::Source::kInlineCode) {
      std::string inline_code = script.js_scripts().at(i)->GetContent().data();
      content->set_content(std::move(inline_code));
    }
  }

  return script_metadata;
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

ExtensionUserScriptLoader::~ExtensionUserScriptLoader() = default;

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
  UserScriptList manifest_scripts =
      GetManifestScriptsMetadata(browser_context(), extension);
  bool has_dynamic_scripts = HasInitialDynamicScripts(extension);

  if (manifest_scripts.empty() && !has_dynamic_scripts) {
    return false;
  }

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
    UserScriptList scripts,
    std::set<std::string> persistent_script_ids,
    DynamicScriptsModifiedCallback callback) {
  // Only proceed with adding scripts that the extension still intends to add.
  // This guards again an edge case where scripts registered by an API call
  // are quickly unregistered.
  std::erase_if(scripts, [&pending_ids = pending_dynamic_script_ids_](
                             const std::unique_ptr<UserScript>& script) {
    return !base::Contains(pending_ids, script->id());
  });

  if (scripts.empty()) {
    std::move(callback).Run(/*error=*/std::nullopt);
    return;
  }

  UserScriptList scripts_to_add;
  for (const auto& script : scripts) {
    // Additionally, only add scripts to the set of active scripts in renderers
    // (through `AddScripts()`) if the `source` for that script is enabled.
    if (!base::Contains(disabled_sources_, script->GetSource())) {
      // TODO(crbug.com/40938420): This results in an additional copy being
      // stored in the browser for each of these scripts. Optimize the usage of
      // inline code.
      scripts_to_add.push_back(CopyDynamicScriptInfo(*script));
    }
  }

  // Note: the sets of `scripts_to_add` and `scripts` are now deliberately
  // different. `scripts_to_add` includes the scripts that should be added to
  // the base `UserScriptLoader`, which then notifies any renderers. `scripts`
  // contains *all* (that weren't unregistered by the extension) so that they
  // are properly serialized and stored for future browser sessions.
  AddScripts(
      std::move(scripts_to_add),
      base::BindOnce(&ExtensionUserScriptLoader::OnDynamicScriptsAdded,
                     weak_factory_.GetWeakPtr(), std::move(scripts),
                     std::move(persistent_script_ids), std::move(callback)));
}

void ExtensionUserScriptLoader::RemoveDynamicScripts(
    const std::set<std::string>& ids_to_remove,
    DynamicScriptsModifiedCallback callback) {
  if (ids_to_remove.empty()) {
    std::move(callback).Run(/*error=*/std::nullopt);
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
    UserScript::Source source,
    DynamicScriptsModifiedCallback callback) {
  RemoveDynamicScripts(GetDynamicScriptIDs(source), std::move(callback));
}

void ExtensionUserScriptLoader::SetSourceEnabled(UserScript::Source source,
                                                 bool enabled) {
  bool currently_enabled = disabled_sources_.count(source) == 0;
  if (enabled == currently_enabled) {
    return;  // Nothing's changed; our work here is done.
  }

  if (enabled) {
    // Re-enable any previously-disabled scripts.
    disabled_sources_.erase(source);
    UserScriptList scripts_to_add;
    for (const auto& script : loaded_dynamic_scripts_) {
      if (script->GetSource() == source) {
        scripts_to_add.push_back(CopyDynamicScriptInfo(*script));
      }
    }

    if (scripts_to_add.empty()) {
      // There were no registered scripts of the given source. Nothing more to
      // do.
      return;
    }

    // Note: This just adds the scripts (which this object already tracked)
    // back into the base UserScriptLoader, which finishes loading the files (if
    // necessary) and sends them out to relevant renderers. Because the scripts
    // are already loaded, we don't need to do anything after adding them (e.g.
    // no need to re-store them).
    AddScripts(std::move(scripts_to_add), base::DoNothing());
  } else {  // Disabling a source.
    disabled_sources_.insert(source);
    std::set<std::string> ids = GetDynamicScriptIDs(source);
    if (ids.empty()) {
      // No registered scripts with the given source. Nothing more to do.
      return;
    }

    // See comment above: no need for any callback here because the stored
    // scripts are unchanged.
    RemoveScripts(ids, base::DoNothing());
  }
}

void ExtensionUserScriptLoader::UpdateDynamicScripts(
    UserScriptList scripts,
    std::set<std::string> script_ids,
    std::set<std::string> persistent_script_ids,
    ExtensionUserScriptLoader::DynamicScriptsModifiedCallback add_callback) {
  // To guarantee that scripts are updated, they need to be removed then added
  // again. It should be guaranteed that the new scripts are added after the old
  // ones are removed.
  RemoveDynamicScripts(script_ids, /*callback=*/base::DoNothing());

  // Since RemoveDynamicScripts will remove pending script IDs, but
  // AddDynamicScripts will only add scripts that are marked as pending, we must
  // mark `script_ids` as pending again here.
  AddPendingDynamicScriptIDs(std::move(script_ids));

  AddDynamicScripts(std::move(scripts), std::move(persistent_script_ids),
                    std::move(add_callback));
}

std::set<std::string> ExtensionUserScriptLoader::GetDynamicScriptIDs(
    UserScript::Source source) const {
  std::set<std::string> dynamic_script_ids;

  for (std::string pending_id : pending_dynamic_script_ids_) {
    if (UserScript::GetSourceForScriptID(pending_id) == source) {
      dynamic_script_ids.insert(pending_id);
    }
  }

  for (const std::unique_ptr<UserScript>& script : loaded_dynamic_scripts_) {
    if (script->GetSource() == source) {
      dynamic_script_ids.insert(script->id());
    }
  }

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

UserScriptList ExtensionUserScriptLoader::LoadScriptsForTest(
    UserScriptList user_scripts) {
  std::set<std::string> added_script_ids;
  for (const std::unique_ptr<UserScript>& script : user_scripts) {
    added_script_ids.insert(script->id());
  }

  UserScriptList result;

  // Block until the scripts have been loaded on the file task runner so that
  // we can return the result synchronously.
  base::RunLoop run_loop;
  LoadScripts(
      std::move(user_scripts), added_script_ids,
      base::BindOnce(
          [](base::OnceClosure done_callback,
             UserScriptList& loaded_user_scripts, UserScriptList user_scripts,
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

    base::Value::Dict value =
        script_serialization::SerializeUserScript(*script).ToValue();
    value.Set(kId, script->id());

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
                                    std::optional<base::Value> value) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id_);
  DCHECK(extension) << "The ExtensionUserScriptLoader should have been cleaned "
                       "up if the extension was disabled";

  UserScriptList scripts;
  if (value && value->is_list()) {
    UserScriptList dynamic_scripts = ConvertValueToScripts(
        *extension, util::IsIncognitoEnabled(extension->id(), browser_context_),
        value->GetList());

    // TODO(crbug.com/40061759): Write back `dynamic_scripts` into the
    // StateStore if scripts in the StateStore do not have prefixed IDs.

    scripts.insert(scripts.end(),
                   std::make_move_iterator(dynamic_scripts.begin()),
                   std::make_move_iterator(dynamic_scripts.end()));
  }

  std::move(callback).Run(std::move(scripts));
}

void ExtensionUserScriptLoader::LoadScripts(
    UserScriptList user_scripts,
    const std::set<std::string>& added_script_ids,
    LoadScriptsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScriptResourceIds script_resource_ids;
  for (const std::unique_ptr<UserScript>& script : user_scripts) {
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
    UserScriptList manifest_scripts,
    UserScriptLoader::ScriptsLoadedCallback callback,
    UserScriptList initial_dynamic_scripts) {
  UserScriptList scripts_to_add = std::move(manifest_scripts);
  for (const std::unique_ptr<UserScript>& script : initial_dynamic_scripts) {
    // Only add the script to the `UserScriptLoader`'s set (thus sending it to
    // renderers) if the script source type is enabled.
    if (!base::Contains(disabled_sources_, script->GetSource())) {
      scripts_to_add.push_back(CopyDynamicScriptInfo(*script));
      pending_dynamic_script_ids_.insert(script->id());
    }
  }

  AddScripts(std::move(scripts_to_add),
             base::BindOnce(
                 &ExtensionUserScriptLoader::OnInitialExtensionScriptsLoaded,
                 weak_factory_.GetWeakPtr(), std::move(initial_dynamic_scripts),
                 std::move(callback)));
}

void ExtensionUserScriptLoader::OnInitialExtensionScriptsLoaded(
    UserScriptList initial_dynamic_scripts,
    UserScriptLoader::ScriptsLoadedCallback callback,
    UserScriptLoader* loader,
    const std::optional<std::string>& error) {
  for (const std::unique_ptr<UserScript>& script : initial_dynamic_scripts) {
    pending_dynamic_script_ids_.erase(script->id());
  }

  if (!error.has_value()) {
    for (const std::unique_ptr<UserScript>& script : initial_dynamic_scripts) {
      persistent_dynamic_script_ids_.insert(script->id());
    }
    loaded_dynamic_scripts_.insert(
        loaded_dynamic_scripts_.end(),
        std::make_move_iterator(initial_dynamic_scripts.begin()),
        std::make_move_iterator(initial_dynamic_scripts.end()));
  }

  std::move(callback).Run(loader, error);
}

void ExtensionUserScriptLoader::OnDynamicScriptsAdded(
    UserScriptList added_scripts,
    std::set<std::string> new_persistent_script_ids,
    DynamicScriptsModifiedCallback callback,
    UserScriptLoader* loader,
    const std::optional<std::string>& error) {
  // Now that a script load for all scripts contained in `added_scripts` has
  // occurred, add these scripts to `loaded_dynamic_scripts_` and remove any ids
  // in `pending_dynamic_script_ids_` that correspond to a script in
  // `added_scripts`.
  for (const std::unique_ptr<UserScript>& script : added_scripts) {
    pending_dynamic_script_ids_.erase(script->id());
  }

  if (!error.has_value()) {
    loaded_dynamic_scripts_.insert(
        loaded_dynamic_scripts_.end(),
        std::make_move_iterator(added_scripts.begin()),
        std::make_move_iterator(added_scripts.end()));

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
    const std::optional<std::string>& error) {
  // Remove scripts from `loaded_dynamic_scripts_` only when the set of
  // `removed_script_ids` have actually been removed and the corresponding IPC
  // has been sent.
  if (!error.has_value()) {
    std::erase_if(
        loaded_dynamic_scripts_,
        [&removed_script_ids](const std::unique_ptr<UserScript>& script) {
          return base::Contains(removed_script_ids, script->id());
        });

    std::erase_if(persistent_dynamic_script_ids_,
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
  bool has_users_scripts_permission =
      extension.permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kUserScripts);
  if (!has_scripting_permission && !has_users_scripts_permission) {
    return false;
  }

  URLPatternSet initial_dynamic_patterns =
      scripting::GetPersistentScriptURLPatterns(browser_context(),
                                                extension.id());
  return !initial_dynamic_patterns.is_empty();
}

}  // namespace extensions
