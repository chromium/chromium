// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/mime_types_handler.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/pdf/common/pdf_util.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace keys = extensions::manifest_keys;
namespace errors = extensions::manifest_errors;

namespace {

// This has to by in sync with MimeHandlerType enum.
// Note that if multiple versions of quickoffice are installed, the
// higher-indexed entry will clobber earlier entries.
const char* kMIMETypeHandlersAllowlist[] = {
    extension_misc::kPdfExtensionId,
    extension_misc::kQuickOfficeComponentExtensionId,
    extension_misc::kQuickOfficeInternalExtensionId,
    extension_misc::kQuickOfficeExtensionId,
    extension_misc::kMimeHandlerPrivateTestExtensionId};

// Used for UMA stats. Entries should not be renumbered and numeric values
// should never be reused. This corresponds to kMimeTypeHandlersAllowlist.
// Don't forget to update enums.xml when updating these.
enum class MimeHandlerType {
  kPdfExtension = 0,
  kQuickOfficeComponentExtension = 1,
  kQuickOfficeInternalExtension = 2,
  kQuickOfficeExtension = 3,
  kTestExtension = 4,

  kMaxValue = kTestExtension,
};

static_assert(
    std::size(kMIMETypeHandlersAllowlist) ==
        static_cast<size_t>(MimeHandlerType::kMaxValue) + 1,
    "MimeHandlerType enum is not in sync with kMIMETypeHandlersAllowlist.");

constexpr SkColor kQuickOfficeExtensionBackgroundColor =
    SkColorSetRGB(241, 241, 241);

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
const std::vector<std::string>& MimeTypesHandler::GetMIMETypeAllowlist() {
  static base::NoDestructor<std::vector<std::string>> allowlist_vector{
      std::begin(kMIMETypeHandlersAllowlist),
      std::end(kMIMETypeHandlersAllowlist)};
  return *allowlist_vector;
}

MimeTypesHandler::MimeTypesHandler() = default;
MimeTypesHandler::~MimeTypesHandler() = default;

void MimeTypesHandler::AddMIMEType(const std::string& mime_type) {
  mime_type_set_.insert(mime_type);
}

bool MimeTypesHandler::CanHandleMIMEType(const std::string& mime_type) const {
  return base::Contains(mime_type_set_, mime_type);
}

bool MimeTypesHandler::HasPlugin() const {
  return !handler_url_.empty();
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
MimeTypesHandler* MimeTypesHandler::GetHandler(
    const extensions::Extension* extension) {
  MimeTypesHandlerInfo* info = static_cast<MimeTypesHandlerInfo*>(
      extension->GetManifestData(keys::kMimeTypesHandler));
  if (info)
    return &info->handler_;
  return nullptr;
}

MimeTypesHandlerParser::MimeTypesHandlerParser() {
}

MimeTypesHandlerParser::~MimeTypesHandlerParser() {
}

bool MimeTypesHandlerParser::Parse(extensions::Extension* extension,
                                   std::u16string* error) {
  const base::Value* mime_types_value = nullptr;
  if (!extension->manifest()->GetList(keys::kMIMETypes, &mime_types_value)) {
    *error = errors::kInvalidMimeTypesHandler;
    return false;
  }

  auto info = std::make_unique<MimeTypesHandlerInfo>();
  info->handler_.set_extension_id(extension->id());
  for (const auto& entry : mime_types_value->GetList()) {
    if (!entry.is_string()) {
      *error = errors::kInvalidMIMETypes;
      return false;
    }
    info->handler_.AddMIMEType(entry.GetString());
  }

  if (const std::string* mime_types_handler =
          extension->manifest()->FindStringPath(keys::kMimeTypesHandler)) {
    info->handler_.set_handler_url(*mime_types_handler);
  }

  extension->SetManifestData(keys::kMimeTypesHandler, std::move(info));
  return true;
}

base::span<const char* const> MimeTypesHandlerParser::Keys() const {
  static constexpr const char* kKeys[] = {keys::kMIMETypes,
                                          keys::kMimeTypesHandler};
  return kKeys;
}
