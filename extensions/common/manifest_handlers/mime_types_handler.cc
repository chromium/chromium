// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/mime_types_handler.h"

#include <stddef.h>

#include <algorithm>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/pdf/common/pdf_util.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/common/api/mime_handlers.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "mime_types_handler.h"

namespace keys = extensions::manifest_keys;
namespace errors = extensions::manifest_errors;

namespace {

// This has to be in sync with MimeHandlerType enum.
//
// This array defines the set of allowlisted MIME handler extension IDs and
// their relative precedence as consumed by `MimeHandlerRegistry`. Precedence
// runs in three tiers: (1) public (non-allowlisted) handlers beat
// allowlisted ones; (2) among public handlers the most recently installed
// wins; (3) among allowlisted handlers a higher index in this array wins.
// As a result, if multiple allowlisted versions of quickoffice are
// installed the higher-indexed entry wins.
constexpr const char* kMIMETypeHandlersAllowlist[] = {
    extension_misc::kPdfExtensionId,
#if BUILDFLAG(IS_CHROMEOS)
    extension_misc::kQuickOfficeComponentExtensionId,
#endif
    extension_misc::kQuickOfficeInternalExtensionId,
    extension_misc::kQuickOfficeExtensionId,
    extension_misc::kMimeHandlerPrivateTestExtensionId};

constexpr SkColor kQuickOfficeExtensionBackgroundColor =
    SkColorSetRGB(241, 241, 241);

// Parses the dict-format `mime_types_handler` value into `handler`.
// Structural errors (non-dict entry value, missing required `handler_url`
// field) are hard errors - they indicate a malformed manifest. Semantic
// errors (unsupported MIME type, bad URL) emit install warnings and skip
// the entry for forward compatibility.
// Returns false and sets `error` on structural failure; returns true
// (possibly with warnings) otherwise.
bool ParseDictFormat(extensions::Extension* extension,
                     const base::DictValue& dict,
                     MimeTypesHandler& handler,
                     std::u16string* error) {
  using ConfigType = extensions::api::mime_handlers::MimeHandlerMimeTypeConfig;

  // Extensions from allow list are allowed to register as any mime type.
  const bool is_legacy_extension =
      std::ranges::contains(kMIMETypeHandlersAllowlist, extension->id());

  for (const auto [mime_type, entry_value] : dict) {
    auto config = ConfigType::FromValue(entry_value);
    if (!config.has_value()) {
      *error = errors::kInvalidMimeTypesHandler;
      return false;
    }

    if (mime_type.empty()) {
      extension->AddInstallWarning(extensions::InstallWarning(
          "mime_types_handler: ignoring entry with empty MIME type key."));
      continue;
    }
    if (!is_legacy_extension &&
        !std::ranges::contains(MimeTypesHandler::GetPublicAllowedMIMETypeList(),
                               mime_type)) {
      extension->AddInstallWarning(extensions::InstallWarning(
          base::StrCat({"mime_types_handler: ignoring unsupported "
                        "MIME type '",
                        mime_type, "'."})));
      continue;
    }
    if (config->handler_url.empty()) {
      extension->AddInstallWarning(extensions::InstallWarning(
          base::StrCat({"mime_types_handler: ignoring entry for '", mime_type,
                        "': empty handler_url."})));
      continue;
    }
    GURL handler_gurl = extension->GetResourceURL(config->handler_url);
    if (!handler_gurl.is_valid()) {
      extension->AddInstallWarning(extensions::InstallWarning(
          base::StrCat({"mime_types_handler: ignoring entry for '", mime_type,
                        "': invalid handler_url."})));
      continue;
    }
    handler.AddMIMEType(mime_type, handler_gurl,
                        config->can_embed.value_or(false));
  }
  return true;
}

// Stored on the Extension.
struct MimeTypesHandlerInfo : public extensions::Extension::ManifestData {
  MimeTypesHandler handler_;

  MimeTypesHandlerInfo();
  ~MimeTypesHandlerInfo() override;
};

MimeTypesHandlerInfo::MimeTypesHandlerInfo() = default;
MimeTypesHandlerInfo::~MimeTypesHandlerInfo() = default;

}  // namespace

// static
const std::vector<extensions::ExtensionId>&
MimeTypesHandler::GetMIMETypeAllowlist() {
  static base::NoDestructor<std::vector<extensions::ExtensionId>>
      allowlist_vector{std::begin(kMIMETypeHandlersAllowlist),
                       std::end(kMIMETypeHandlersAllowlist)};
  return *allowlist_vector;
}

// static
base::span<const std::string_view>
MimeTypesHandler::GetPublicAllowedMIMETypeList() {
  static constexpr std::string_view kAllowed[] = {"application/pdf"};
  return kAllowed;
}

MimeTypesHandler::MimeTypeConfig::MimeTypeConfig() = default;
MimeTypesHandler::MimeTypeConfig::MimeTypeConfig(const MimeTypeConfig&) =
    default;
MimeTypesHandler::MimeTypeConfig& MimeTypesHandler::MimeTypeConfig::operator=(
    const MimeTypeConfig&) = default;
MimeTypesHandler::MimeTypeConfig::~MimeTypeConfig() = default;

MimeTypesHandler::MimeTypesHandler() = default;
MimeTypesHandler::~MimeTypesHandler() = default;

void MimeTypesHandler::AddMIMEType(const std::string& mime_type,
                                   const GURL& handler_url,
                                   bool can_embed) {
  auto& config = per_type_configs_[mime_type];
  config.handler_url = handler_url;
  config.can_embed = can_embed;
}

std::vector<std::string> MimeTypesHandler::GetSupportedMimeTypes() const {
  std::vector<std::string> result;
  result.reserve(per_type_configs_.size());
  for (const auto& entry : per_type_configs_) {
    result.emplace_back(entry.first);
  }
  return result;
}

bool MimeTypesHandler::IsPluginExtension() const {
  return std::ranges::contains(kMIMETypeHandlersAllowlist, extension_id_);
}

GURL MimeTypesHandler::GetHandlerUrl(const std::string& mime_type) const {
  auto it = per_type_configs_.find(mime_type);
  return it != per_type_configs_.end() ? it->second.handler_url : GURL();
}

bool MimeTypesHandler::CanEmbedMimeType(const std::string& mime_type) const {
  auto it = per_type_configs_.find(mime_type);
  return it != per_type_configs_.end() && it->second.can_embed;
}

bool MimeTypesHandler::HasPlugin() const {
  // For plugin (legacy) handlers all MIME types share the same handler URL,
  // so checking the first entry is sufficient to determine if a URL is set.
  return IsPluginExtension() && !per_type_configs_.empty() &&
         per_type_configs_.begin()->second.handler_url.is_valid();
}

SkColor MimeTypesHandler::GetBackgroundColor() const {
  if (extension_id_ == extension_misc::kPdfExtensionId) {
    return GetPdfBackgroundColor();
  }
  if (extension_misc::IsQuickOfficeExtension(extension_id_)) {
    return kQuickOfficeExtensionBackgroundColor;
  }
  return content::WebPluginInfo::kDefaultBackgroundColor;
}

base::FilePath MimeTypesHandler::GetPluginPath() const {
  // TODO(raymes): Storing the extension URL in a base::FilePath is really
  // nasty. We should probably just use the extension ID as the placeholder path
  // instead.
  return base::FilePath::FromUTF8Unsafe(
      std::string(extensions::kExtensionScheme) + "://" + extension_id_ + "/");
}

// static
const MimeTypesHandler* MimeTypesHandler::Get(
    const extensions::Extension& extension) {
  const MimeTypesHandlerInfo* info = static_cast<const MimeTypesHandlerInfo*>(
      extension.GetManifestData(keys::kMimeTypesHandler));
  if (info) {
    return &info->handler_;
  }
  return nullptr;
}

MimeTypesHandlerParser::MimeTypesHandlerParser() = default;
MimeTypesHandlerParser::~MimeTypesHandlerParser() = default;

bool MimeTypesHandlerParser::Parse(extensions::Extension* extension,
                                   std::u16string* error) {
  // Check if mime_types_handler is a dict (new format). Dict format stores
  // per-type config directly, so the separate "mime_types" list is not needed.
  const base::Value* handler_value =
      extension->manifest()->FindPath(keys::kMimeTypesHandler);
  if (handler_value && handler_value->is_dict()) {
    if (!base::FeatureList::IsEnabled(extensions_features::kApiMimeHandler)) {
      return true;
    }

    auto info = std::make_unique<MimeTypesHandlerInfo>();
    info->handler_.set_extension_id(extension->id());

    if (!ParseDictFormat(extension, handler_value->GetDict(), info->handler_,
                         error)) {
      return false;
    }

    if (info->handler_.GetSupportedMimeTypes().empty()) {
      return true;
    }

    extension->SetManifestData(keys::kMimeTypesHandler, std::move(info));
    return true;
  }

  // Legacy format: "mime_types" list + "mime_types_handler" string.
  const base::Value* mime_types_value = nullptr;
  if (!extension->manifest()->GetList(keys::kMIMETypes, &mime_types_value)) {
    *error = errors::kInvalidMimeTypesHandler;
    return false;
  }

  std::vector<std::string> mime_types;
  for (const auto& entry : mime_types_value->GetList()) {
    if (!entry.is_string()) {
      *error = errors::kInvalidMIMETypes;
      return false;
    }
    mime_types.emplace_back(entry.GetString());
  }

  GURL handler_gurl;
  if (const std::string* handler_url =
          extension->manifest()->FindStringPath(keys::kMimeTypesHandler)) {
    handler_gurl = extension->GetResourceURL(*handler_url);
  }

  auto info = std::make_unique<MimeTypesHandlerInfo>();
  info->handler_.set_extension_id(extension->id());
  for (const std::string& mime_type : mime_types) {
    info->handler_.AddMIMEType(mime_type, handler_gurl, /*can_embed=*/false);
  }

  extension->SetManifestData(keys::kMimeTypesHandler, std::move(info));
  return true;
}

base::span<const char* const> MimeTypesHandlerParser::Keys() const {
  static constexpr const char* kKeys[] = {keys::kMIMETypes,
                                          keys::kMimeTypesHandler};
  return kKeys;
}
