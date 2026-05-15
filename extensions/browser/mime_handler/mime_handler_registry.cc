// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_registry.h"

#include <algorithm>

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/install_prefs_helper.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"

namespace extensions {

namespace {

// Per-extension dict mapping `mime_type` -> bool (enabled flag).
// Missing entries mean "enabled" (the default).
constexpr PrefMap kMimeHandlerEnabled = {"mime_handler_enabled",
                                         PrefType::kDictionary,
                                         PrefScope::kExtensionSpecific};

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
  observation_.Observe(ExtensionRegistry::Get(context));

  // Register already-loaded extensions.
  for (const auto& extension :
       ExtensionRegistry::Get(context)->enabled_extensions()) {
    RegisterExtension(extension.get());
  }
}

MimeHandlerRegistry::~MimeHandlerRegistry() = default;

ExtensionId MimeHandlerRegistry::GetHandlerForMimeType(
    const std::string& mime_type) const {
  base::span<const ExtensionId> candidates = GetHandlersForMimeType(mime_type);
  return candidates.empty() ? ExtensionId() : candidates.front();
}

base::span<const ExtensionId> MimeHandlerRegistry::GetHandlersForMimeType(
    const std::string& mime_type) const {
  auto it = handlers_by_type_.find(mime_type);
  if (it == handlers_by_type_.end()) {
    return {};
  }
  CHECK(!it->second.empty());
  return it->second;
}

const MimeHandlerRegistry::HandlersByMimeType&
MimeHandlerRegistry::GetHandlersByMimeType() const {
  return handlers_by_type_;
}

bool MimeHandlerRegistry::IsEnabledForMimeType(
    const ExtensionId& extension_id,
    const std::string& mime_type) const {
  const base::DictValue* dict =
      ExtensionPrefs::Get(&*browser_context_)
          ->ReadPrefAsDictionary(extension_id, kMimeHandlerEnabled);
  return !dict || dict->FindBool(mime_type).value_or(true);
}

void MimeHandlerRegistry::SetEnabledForMimeType(const ExtensionId& extension_id,
                                                const std::string& mime_type,
                                                bool enabled) {
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
  base::DictValue dict;
  if (const base::DictValue* existing =
          prefs->ReadPrefAsDictionary(extension_id, kMimeHandlerEnabled)) {
    dict = existing->Clone();
  }
  dict.Set(mime_type, enabled);
  prefs->SetDictionaryPref(extension_id, kMimeHandlerEnabled, std::move(dict));

  // Mirror the new state in `handlers_by_type_` so lookups don't need
  // to consult prefs. The `mimeHandler` API is gated on
  // `manifest:mime_types_handler` and disabled-extension calls are
  // dropped before reaching here, so the calling extension is loaded
  // and has a `MimeTypesHandler`. The manifest may still not claim
  // `mime_type` (the API takes an arbitrary string), so that case is
  // a no-op.
  if (enabled) {
    const Extension* extension = ExtensionRegistry::Get(&*browser_context_)
                                     ->enabled_extensions()
                                     .GetByID(extension_id);
    CHECK(extension);
    const MimeTypesHandler* handler = MimeTypesHandler::Get(*extension);
    CHECK(handler);
    const auto& mime_types = handler->GetSupportedMimeTypes();
    if (std::ranges::find(mime_types, mime_type) == mime_types.end()) {
      return;
    }
    std::vector<ExtensionId>& handlers = handlers_by_type_[mime_type];
    if (std::ranges::find(handlers, extension_id) == handlers.end()) {
      handlers.emplace_back(extension_id);
      SortByPrecedence(handlers);
    }
    return;
  }

  auto it = handlers_by_type_.find(mime_type);
  if (it == handlers_by_type_.end()) {
    return;
  }
  std::erase(it->second, extension_id);
  base::EraseIf(handlers_by_type_,
                [](const auto& pair) { return pair.second.empty(); });
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
    if (!IsEnabledForMimeType(extension->id(), mime_type)) {
      // Disabled handlers are excluded from the registry entirely.
      // `SetEnabledForMimeType` will add them back if re-enabled.
      continue;
    }
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
