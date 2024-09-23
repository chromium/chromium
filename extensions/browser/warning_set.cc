// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/warning_set.h"

#include <stddef.h>

#include <tuple>

#include "base/files/file_path.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extensions_client.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {
// Prefix for message parameters indicating that the parameter needs to
// be translated from an extension id to the extension name.
const char kTranslate[] = "TO_TRANSLATE:";
const size_t kMaxNumberOfParameters = 4;
}

namespace extensions {

//
// Warning
//

Warning::Warning(WarningType type,
                 const ExtensionId& extension_id,
                 int message_id,
                 const std::vector<std::string>& message_parameters)
    : type_(type),
      extension_id_(extension_id),
      message_id_(message_id),
      message_parameters_(message_parameters) {
  // These are invalid here because they do not have corresponding warning
  // messages in the UI.
  CHECK_NE(type, kInvalid);
  CHECK_NE(type, kMaxWarningType);
  CHECK_LE(message_parameters.size(), kMaxNumberOfParameters);
}

Warning::Warning(const Warning& other)
  : type_(other.type_),
    extension_id_(other.extension_id_),
    message_id_(other.message_id_),
    message_parameters_(other.message_parameters_) {}

Warning::~Warning() {
}

Warning& Warning::operator=(const Warning& other) {
  type_ = other.type_;
  extension_id_ = other.extension_id_;
  message_id_ = other.message_id_;
  message_parameters_ = other.message_parameters_;
  return *this;
}

// static
Warning Warning::CreateNetworkDelayWarning(const ExtensionId& extension_id) {
  std::vector<std::string> message_parameters;
  message_parameters.push_back(ExtensionsClient::Get()->GetProductName());
  return Warning(
      kNetworkDelay,
      extension_id,
      IDS_EXTENSION_WARNINGS_NETWORK_DELAY,
      message_parameters);
}

// static
Warning Warning::CreateRepeatedCacheFlushesWarning(
    const ExtensionId& extension_id) {
  std::vector<std::string> message_parameters;
  message_parameters.push_back(ExtensionsClient::Get()->GetProductName());
  return Warning(
      kRepeatedCacheFlushes,
      extension_id,
      IDS_EXTENSION_WARNINGS_NETWORK_DELAY,
      message_parameters);
}

// static
Warning Warning::CreateDownloadFilenameConflictWarning(
    const std::string& losing_extension_id,
    const std::string& winning_extension_id,
    const base::FilePath& losing_filename,
    const base::FilePath& winning_filename) {
  std::vector<std::string> message_parameters;
  message_parameters.push_back(base::UTF16ToUTF8(
      losing_filename.LossyDisplayName()));
  message_parameters.push_back(kTranslate + winning_extension_id);
  message_parameters.push_back(base::UTF16ToUTF8(
      winning_filename.LossyDisplayName()));
  return Warning(
      kDownloadFilenameConflict,
      losing_extension_id,
      IDS_EXTENSION_WARNINGS_DOWNLOAD_FILENAME_CONFLICT,
      message_parameters);
}

// static
Warning Warning::CreateReloadTooFrequentWarning(
    const ExtensionId& extension_id) {
  std::vector<std::string> message_parameters;
  return Warning(kReloadTooFrequent,
                          extension_id,
                          IDS_EXTENSION_WARNING_RELOAD_TOO_FREQUENT,
                          message_parameters);
}

// static
Warning Warning::CreateRulesetFailedToLoadWarning(
    const ExtensionId& extension_id) {
  return Warning(kRulesetFailedToLoad, extension_id,
                 IDS_EXTENSION_WARNING_RULESET_FAILED_TO_LOAD,
                 {} /*message_parameters*/);
}

// static
Warning Warning::CreateEnabledRuleCountExceededWarning(
    const ExtensionId& extension_id) {
  return Warning(kEnabledRuleCountExceeded, extension_id,
                 IDS_EXTENSION_WARNING_ENABLED_RULE_COUNT_EXCEEDED,
                 {} /*message_parameters*/);
}

bool Warning::operator<(const Warning& other) const {
  return std::tie(extension_id_, type_) <
         std::tie(other.extension_id_, other.type_);
}

std::string Warning::GetLocalizedMessage(const ExtensionSet* extensions) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // These parameters may be unsafe (URLs and Extension names) and need
  // to be HTML-escaped before being embedded in the UI. Also extension IDs
  // are translated to full extension names.
  std::vector<std::u16string> final_parameters;
  for (size_t i = 0; i < message_parameters_.size(); ++i) {
    std::string message = message_parameters_[i];
    if (base::StartsWith(message, kTranslate, base::CompareCase::SENSITIVE)) {
      ExtensionId extension_id = message.substr(sizeof(kTranslate) - 1);
      const extensions::Extension* extension =
          extensions->GetByID(extension_id);
      message = extension ? extension->name() : extension_id;
    }
    final_parameters.push_back(base::UTF8ToUTF16(base::EscapeForHTML(message)));
  }

  static_assert(kMaxNumberOfParameters == 4u,
                "You Need To Add More Case Statements");
  switch (final_parameters.size()) {
    case 0:
      return l10n_util::GetStringUTF8(message_id_);
    case 1:
      return l10n_util::GetStringFUTF8(message_id_, final_parameters[0]);
    case 2:
      return l10n_util::GetStringFUTF8(message_id_, final_parameters[0],
          final_parameters[1]);
    case 3:
      return l10n_util::GetStringFUTF8(message_id_, final_parameters[0],
          final_parameters[1], final_parameters[2]);
    case 4:
      return l10n_util::GetStringFUTF8(message_id_, final_parameters[0],
          final_parameters[1], final_parameters[2], final_parameters[3]);
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

}  // namespace extensions
