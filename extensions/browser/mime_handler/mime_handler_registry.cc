// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_registry.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/containers/map_util.h"
#include "base/no_destructor.h"
#include "components/crx_file/id_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_lifetime_tracker.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/install_prefs_helper.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"

namespace extensions {

namespace {

// Per-extension dict mapping `mime_type` -> options record.
constexpr PrefMap kMimeHandlerOptions = {"mime_handler_options",
                                         PrefType::kDictionary,
                                         PrefScope::kExtensionSpecific};

// Keys inside the per-MIME-type record of `kMimeHandlerOptions`. Each key
// holds an option value that the extension chose at runtime.
constexpr char kMimeHandlerEnabledKey[] = "enabled";
constexpr char kMimeHandlerCanEmbedKey[] = "canEmbed";

class MimeHandlerRegistryFactory : public BrowserContextKeyedServiceFactory {
 public:
  MimeHandlerRegistryFactory(const MimeHandlerRegistryFactory&) = delete;
  MimeHandlerRegistryFactory& operator=(const MimeHandlerRegistryFactory&) =
      delete;

  static MimeHandlerRegistryFactory* GetInstance() {
    static base::NoDestructor<MimeHandlerRegistryFactory> instance;
    return instance.get();
  }

  static MimeHandlerRegistry* GetForBrowserContext(
      content::BrowserContext* context) {
    return static_cast<MimeHandlerRegistry*>(
        GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
  }

 private:
  friend class base::NoDestructor<MimeHandlerRegistryFactory>;

  MimeHandlerRegistryFactory()
      : BrowserContextKeyedServiceFactory(
            "MimeHandlerRegistry",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(ExtensionRegistryFactory::GetInstance());
    DependsOn(ExtensionPrefsFactory::GetInstance());
  }

  ~MimeHandlerRegistryFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override {
    return std::make_unique<MimeHandlerRegistry>(context);
  }

  bool ServiceIsCreatedWithBrowserContext() const override { return true; }

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
        context);
  }
};

// Moves the per-MIME-type bools stored under "mime_handler_enabled" into
// `kMimeHandlerOptions` records, then deletes the old key.
// TODO(crbug.com/495538206): Remove after M160.
void MigrateMimeHandlerEnabledToOptions(ExtensionPrefs& prefs) {
  constexpr char kObsoleteMimeHandlerEnabledPref[] = "mime_handler_enabled";

  const base::DictValue& extensions =
      prefs.pref_service()->GetDict(pref_names::kExtensions);
  for (const auto [extension_id, _] : extensions) {
    if (!crx_file::id_util::IdIsValid(extension_id)) {
      continue;
    }
    const base::DictValue* enabled_by_mime_type =
        prefs.ReadPrefAsDict(extension_id, kObsoleteMimeHandlerEnabledPref);
    if (!enabled_by_mime_type) {
      continue;
    }

    base::DictValue options;
    for (const auto [mime_type, value] : *enabled_by_mime_type) {
      if (value.is_bool()) {
        options.EnsureDict(mime_type)->Set(kMimeHandlerEnabledKey,
                                           value.GetBool());
      }
    }
    if (!options.empty()) {
      prefs.SetDictionaryPref(extension_id, kMimeHandlerOptions,
                              std::move(options));
    }
    prefs.UpdateExtensionPref(extension_id, kObsoleteMimeHandlerEnabledPref,
                              std::nullopt);
  }
}

// Returns a copy of `options_by_mime_type` with only the MIME types that
// `handler` declares. Returns an empty dict if `handler` is null.
base::DictValue CopyOptionsOfDeclaredMimeTypes(
    const base::DictValue& options_by_mime_type,
    const MimeTypesHandler* handler) {
  const std::vector<std::string> declared_mime_types =
      handler ? handler->GetSupportedMimeTypes() : std::vector<std::string>();
  base::DictValue kept_options = options_by_mime_type.Clone();
  for (const auto [mime_type, _] : options_by_mime_type) {
    if (!std::ranges::contains(declared_mime_types, mime_type)) {
      kept_options.Remove(mime_type);
    }
  }
  return kept_options;
}

// Returns nullopt if `options_by_mime_type` is null or holds no `key` value for
// `mime_type`.
std::optional<bool> FindOptionInMimeTypeRecord(
    const base::DictValue* options_by_mime_type,
    const std::string& mime_type,
    std::string_view key) {
  const base::DictValue* record =
      options_by_mime_type ? options_by_mime_type->FindDict(mime_type)
                           : nullptr;
  return record ? record->FindBool(key) : std::nullopt;
}

// Returns true if `context` is off-the-record and the extension with
// `extension_id` runs in split incognito mode. The registry keeps separate
// incognito options for such an extension.
bool ShouldUseIncognitoOptions(content::BrowserContext* context,
                               const ExtensionId& extension_id) {
  if (!context->IsOffTheRecord()) {
    return false;
  }
  const Extension* extension =
      ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          extension_id);
  CHECK(extension);
  return IncognitoInfo::IsSplitMode(extension);
}

}  // namespace

// static
MimeHandlerRegistry* MimeHandlerRegistry::Get(
    content::BrowserContext* context) {
  return MimeHandlerRegistryFactory::GetForBrowserContext(context);
}

// static
void MimeHandlerRegistry::EnsureFactoryBuilt() {
  MimeHandlerRegistryFactory::GetInstance();
}

MimeHandlerRegistry::MimeHandlerRegistry(content::BrowserContext* context)
    : browser_context_(*context),
      browser_context_lifetime_tracker_(
          ExtensionsBrowserClient::Get()->GetBrowserContextLifetimeTracker()) {
  MigrateMimeHandlerEnabledToOptions(*ExtensionPrefs::Get(context));

  observation_.Observe(ExtensionRegistry::Get(context));

  // Register already-loaded extensions.
  for (const auto& extension :
       ExtensionRegistry::Get(context)->enabled_extensions()) {
    RegisterExtension(extension.get());
  }

  if (browser_context_lifetime_tracker_) {
    browser_context_lifetime_tracker_->StartObserving(*context, *this);
  }
}

MimeHandlerRegistry::~MimeHandlerRegistry() {
  if (browser_context_lifetime_tracker_) {
    browser_context_lifetime_tracker_->StopObserving(*this);
  }
}

std::vector<ExtensionId> MimeHandlerRegistry::GetHandlersForMimeType(
    const std::string& mime_type,
    content::BrowserContext* context) const {
  auto it = handlers_by_type_.find(mime_type);
  if (it == handlers_by_type_.end()) {
    return {};
  }
  CHECK(!it->second.empty());
  return EnabledHandlers(mime_type, it->second, context);
}

MimeHandlerRegistry::HandlersByMimeType
MimeHandlerRegistry::GetHandlersByMimeType(
    content::BrowserContext* context) const {
  HandlersByMimeType enabled_handlers_by_type;
  for (const auto& [mime_type, handlers] : handlers_by_type_) {
    std::vector<ExtensionId> enabled_handlers =
        EnabledHandlers(mime_type, handlers, context);
    if (!enabled_handlers.empty()) {
      enabled_handlers_by_type[mime_type] = std::move(enabled_handlers);
    }
  }
  return enabled_handlers_by_type;
}

bool MimeHandlerRegistry::IsEnabledForMimeType(
    const ExtensionId& extension_id,
    const std::string& mime_type,
    content::BrowserContext* context) const {
  const MimeTypesHandler& handler =
      GetHandlerOfMimeType(extension_id, mime_type);

  std::optional<bool> stored = ReadStoredOption(
      extension_id, mime_type, kMimeHandlerEnabledKey, context);
  if (stored.has_value()) {
    return stored.value();
  }
  return handler.EnabledByDefault(mime_type);
}

bool MimeHandlerRegistry::CanEmbedForMimeType(
    const ExtensionId& extension_id,
    const std::string& mime_type,
    content::BrowserContext* context) const {
  const MimeTypesHandler& handler =
      GetHandlerOfMimeType(extension_id, mime_type);

  std::optional<bool> stored = ReadStoredOption(
      extension_id, mime_type, kMimeHandlerCanEmbedKey, context);
  if (stored.has_value()) {
    return stored.value();
  }
  return handler.CanEmbedMimeType(mime_type);
}

void MimeHandlerRegistry::SetEnabledForMimeType(
    const ExtensionId& extension_id,
    const std::string& mime_type,
    bool enabled,
    content::BrowserContext* context) {
  // Called only to validate the input. The `mimeHandler` API is gated on
  // `manifest:mime_types_handler` and disabled-extension calls are dropped
  // before reaching here, so the calling extension is loaded and has a
  // `MimeTypesHandler`.
  GetHandlerOfMimeType(extension_id, mime_type);

  StoreOption(extension_id, mime_type, kMimeHandlerEnabledKey, enabled,
              context);
}

void MimeHandlerRegistry::SetCanEmbedForMimeType(
    const ExtensionId& extension_id,
    const std::string& mime_type,
    bool can_embed,
    content::BrowserContext* context) {
  // Called only to validate the input. See SetEnabledForMimeType().
  GetHandlerOfMimeType(extension_id, mime_type);
  StoreOption(extension_id, mime_type, kMimeHandlerCanEmbedKey, can_embed,
              context);
}

void MimeHandlerRegistry::OnRelatedOffTheRecordBrowserContextDestroyed(
    content::BrowserContext& off_the_record_context) {
  incognito_options_.clear();
}

void MimeHandlerRegistry::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  // The handler is missing when an update removes every MIME type from the
  // manifest.
  const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(&*browser_context_);
  if (const base::DictValue* options =
          prefs->ReadPrefAsDictionary(extension->id(), kMimeHandlerOptions)) {
    prefs->SetDictionaryPref(extension->id(), kMimeHandlerOptions,
                             CopyOptionsOfDeclaredMimeTypes(*options, handler));
  }
  if (base::DictValue* incognito_options =
          base::FindOrNull(incognito_options_, extension->id())) {
    *incognito_options =
        CopyOptionsOfDeclaredMimeTypes(*incognito_options, handler);
  }
}

void MimeHandlerRegistry::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  RegisterExtension(extension);
}

void MimeHandlerRegistry::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  UnregisterExtension(extension->id());
}

void MimeHandlerRegistry::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  incognito_options_.erase(extension->id());
}

void MimeHandlerRegistry::RegisterExtension(const Extension* extension) {
  const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
  if (!handler) {
    return;
  }

  for (const auto& mime_type : handler->GetSupportedMimeTypes()) {
    std::vector<ExtensionId>& handlers = handlers_by_type_[mime_type];
    handlers.emplace_back(extension->id());
    SortByPrecedence(handlers);
  }
}

void MimeHandlerRegistry::UnregisterExtension(const ExtensionId& extension_id) {
  for (auto& [mime_type, handlers] : handlers_by_type_) {
    std::erase(handlers, extension_id);
  }
  base::EraseIf(handlers_by_type_,
                [](const auto& pair) { return pair.second.empty(); });
}

const MimeTypesHandler& MimeHandlerRegistry::GetHandlerOfMimeType(
    const ExtensionId& extension_id,
    const std::string& mime_type) const {
  const Extension* extension = ExtensionRegistry::Get(&*browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  CHECK(extension);
  const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
  CHECK(handler);
  CHECK(std::ranges::contains(handler->GetSupportedMimeTypes(), mime_type));
  return *handler;
}

std::vector<ExtensionId> MimeHandlerRegistry::EnabledHandlers(
    const std::string& mime_type,
    const std::vector<ExtensionId>& handlers,
    content::BrowserContext* context) const {
  std::vector<ExtensionId> enabled_handlers = handlers;
  std::erase_if(enabled_handlers, [&](const ExtensionId& extension_id) {
    return !IsEnabledForMimeType(extension_id, mime_type, context);
  });
  return enabled_handlers;
}

std::optional<bool> MimeHandlerRegistry::ReadStoredOption(
    const ExtensionId& extension_id,
    const std::string& mime_type,
    std::string_view key,
    content::BrowserContext* context) const {
  if (ShouldUseIncognitoOptions(context, extension_id)) {
    std::optional<bool> incognito_value = FindOptionInMimeTypeRecord(
        base::FindOrNull(incognito_options_, extension_id), mime_type, key);
    if (incognito_value.has_value()) {
      return incognito_value;
    }
  }
  return FindOptionInMimeTypeRecord(
      ExtensionPrefs::Get(&*browser_context_)
          ->ReadPrefAsDictionary(extension_id, kMimeHandlerOptions),
      mime_type, key);
}

void MimeHandlerRegistry::StoreOption(const ExtensionId& extension_id,
                                      const std::string& mime_type,
                                      std::string_view key,
                                      bool value,
                                      content::BrowserContext* context) {
  if (ShouldUseIncognitoOptions(context, extension_id)) {
    incognito_options_[extension_id].EnsureDict(mime_type)->Set(key, value);
    return;
  }
  ExtensionPrefs* prefs = ExtensionPrefs::Get(&*browser_context_);
  base::DictValue options;
  if (const base::DictValue* existing =
          prefs->ReadPrefAsDictionary(extension_id, kMimeHandlerOptions)) {
    options = existing->Clone();
  }
  options.EnsureDict(mime_type)->Set(key, value);
  prefs->SetDictionaryPref(extension_id, kMimeHandlerOptions,
                           std::move(options));
}

void MimeHandlerRegistry::SortByPrecedence(
    std::vector<ExtensionId>& handlers) const {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(&*browser_context_);
  const std::vector<ExtensionId>& allowlist =
      MimeTypesHandler::GetMIMETypeAllowlist();
  // Returns the index of `id` in `kMIMETypeHandlersAllowlist`, or -1 if
  // `id` is a public (non-allowlisted) handler.
  auto allowlist_index = [&allowlist](const ExtensionId& id) -> int {
    auto it = std::ranges::find(allowlist, id);
    return it == allowlist.end() ? -1
                                 : static_cast<int>(it - allowlist.begin());
  };

  // Sort DESCENDING by precedence so `front()` is the winner:
  //   1. Public (non-allowlisted) handlers beat allowlisted ones.
  //   2. Among public handlers: newest `GetFirstInstallTime` wins.
  //   3. Among allowlisted handlers: higher `kMIMETypeHandlersAllowlist`
  //      array index wins.
  std::ranges::sort(handlers, [prefs, &allowlist_index](const ExtensionId& a,
                                                        const ExtensionId& b) {
    const int ia = allowlist_index(a);
    const int ib = allowlist_index(b);
    const bool a_allow = ia >= 0;
    const bool b_allow = ib >= 0;
    if (a_allow != b_allow) {
      // Public (a_allow == false) sorts before allowlisted.
      return !a_allow;
    }
    if (a_allow) {
      return ia > ib;
    }
    return GetFirstInstallTime(prefs, a) > GetFirstInstallTime(prefs, b);
  });
}

}  // namespace extensions
