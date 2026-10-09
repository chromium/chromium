// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_registry.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/no_destructor.h"
#include "components/crx_file/id_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/install_prefs_helper.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/pref_types.h"
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
    : browser_context_(*context) {
  MigrateMimeHandlerEnabledToOptions(*ExtensionPrefs::Get(context));

  observation_.Observe(ExtensionRegistry::Get(context));

  // Register already-loaded extensions.
  for (const auto& extension :
       ExtensionRegistry::Get(context)->enabled_extensions()) {
    RegisterExtension(extension.get());
  }
}

MimeHandlerRegistry::~MimeHandlerRegistry() = default;

std::vector<ExtensionId> MimeHandlerRegistry::GetHandlersForMimeType(
    const std::string& mime_type) const {
  auto it = handlers_by_type_.find(mime_type);
  if (it == handlers_by_type_.end()) {
    return {};
  }
  CHECK(!it->second.empty());
  return EnabledHandlers(mime_type, it->second);
}

MimeHandlerRegistry::HandlersByMimeType
MimeHandlerRegistry::GetHandlersByMimeType() const {
  HandlersByMimeType enabled_handlers_by_type;
  for (const auto& [mime_type, handlers] : handlers_by_type_) {
    std::vector<ExtensionId> enabled_handlers =
        EnabledHandlers(mime_type, handlers);
    if (!enabled_handlers.empty()) {
      enabled_handlers_by_type[mime_type] = std::move(enabled_handlers);
    }
  }
  return enabled_handlers_by_type;
}

bool MimeHandlerRegistry::IsEnabledForMimeType(
    const ExtensionId& extension_id,
    const std::string& mime_type) const {
  const MimeTypesHandler& handler =
      GetHandlerOfMimeType(extension_id, mime_type);

  const base::DictValue* options =
      ExtensionPrefs::Get(&*browser_context_)
          ->ReadPrefAsDictionary(extension_id, kMimeHandlerOptions);
  if (const base::DictValue* record =
          options ? options->FindDict(mime_type) : nullptr) {
    std::optional<bool> stored = record->FindBool(kMimeHandlerEnabledKey);
    if (stored.has_value()) {
      return stored.value();
    }
  }
  return handler.EnabledByDefault(mime_type);
}

void MimeHandlerRegistry::SetEnabledForMimeType(const ExtensionId& extension_id,
                                                const std::string& mime_type,
                                                bool enabled) {
  // Called only to validate the input. The `mimeHandler` API is gated on
  // `manifest:mime_types_handler` and disabled-extension calls are dropped
  // before reaching here, so the calling extension is loaded and has a
  // `MimeTypesHandler`.
  GetHandlerOfMimeType(extension_id, mime_type);

  // TODO(crbug.com/495538206): Define behavior for two cases not yet
  // covered by the spec:
  //   1. Persistence across extension updates — prefs are keyed by
  //      extension id and survive updates, so entries for MIME types
  //      the new manifest no longer claims become stale.
  //   2. Split-mode incognito — ExtensionPrefs are not split by
  //      profile mode, so an incognito-side write here silently
  //      overwrites the on-record setting. In-memory, profile-keyed
  //      storage for OTR may be the right answer.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(&*browser_context_);
  base::DictValue options;
  if (const base::DictValue* existing =
          prefs->ReadPrefAsDictionary(extension_id, kMimeHandlerOptions)) {
    options = existing->Clone();
  }
  options.EnsureDict(mime_type)->Set(kMimeHandlerEnabledKey, enabled);
  prefs->SetDictionaryPref(extension_id, kMimeHandlerOptions,
                           std::move(options));
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
    const std::vector<ExtensionId>& handlers) const {
  std::vector<ExtensionId> enabled_handlers = handlers;
  std::erase_if(enabled_handlers, [&](const ExtensionId& extension_id) {
    return !IsEnabledForMimeType(extension_id, mime_type);
  });
  return enabled_handlers;
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
