// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/utils.h"

#include <memory>
#include <set>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace extensions {
namespace declarative_net_request {
namespace {

namespace dnr_api = api::declarative_net_request;

// The ruleset format version of the flatbuffer schema. Increment this whenever
// making an incompatible change to the schema at extension_ruleset.fbs or
// url_pattern_index.fbs. Whenever an extension with an indexed ruleset format
// version different from the one currently used by Chrome is loaded, the
// extension ruleset will be reindexed.
constexpr int kIndexedRulesetFormatVersion = 12;

// This static assert is meant to catch cases where
// url_pattern_index::kUrlPatternIndexFormatVersion is incremented without
// updating kIndexedRulesetFormatVersion.
static_assert(url_pattern_index::kUrlPatternIndexFormatVersion == 5,
              "kUrlPatternIndexFormatVersion has changed, make sure you've "
              "also updated kIndexedRulesetFormatVersion above.");

constexpr int kInvalidIndexedRulesetFormatVersion = -1;

int g_indexed_ruleset_format_version_for_testing =
    kInvalidIndexedRulesetFormatVersion;

int GetIndexedRulesetFormatVersion() {
  return g_indexed_ruleset_format_version_for_testing ==
                 kInvalidIndexedRulesetFormatVersion
             ? kIndexedRulesetFormatVersion
             : g_indexed_ruleset_format_version_for_testing;
}

// Returns the header to be used for indexed rulesets. This depends on the
// current ruleset format version.
std::string GetVersionHeader() {
  return base::StringPrintf("---------Version=%d",
                            GetIndexedRulesetFormatVersion());
}

// Returns the checksum of the given serialized |data|. |data| must not include
// the version header.
int GetChecksum(base::span<const uint8_t> data) {
  uint32_t hash = base::PersistentHash(data.data(), data.size());

  // Strip off the sign bit since this needs to be persisted in preferences
  // which don't support unsigned ints.
  return static_cast<int>(hash & 0x7fffffff);
}

void ClearRendererCacheOnUI() {
  web_cache::WebCacheManager::GetInstance()->ClearCacheOnNavigation();
}

}  // namespace

bool IsValidRulesetData(base::span<const uint8_t> data, int expected_checksum) {
  flatbuffers::Verifier verifier(data.data(), data.size());
  return expected_checksum == GetChecksum(data) &&
         flat::VerifyExtensionIndexedRulesetBuffer(verifier);
}

std::string GetVersionHeaderForTesting() {
  return GetVersionHeader();
}

int GetIndexedRulesetFormatVersionForTesting() {
  return GetIndexedRulesetFormatVersion();
}

void SetIndexedRulesetFormatVersionForTesting(int version) {
  DCHECK_NE(kInvalidIndexedRulesetFormatVersion, version);
  g_indexed_ruleset_format_version_for_testing = version;
}

bool StripVersionHeaderAndParseVersion(std::string* ruleset_data) {
  DCHECK(ruleset_data);
  const std::string version_header = GetVersionHeader();

  if (!base::StartsWith(*ruleset_data, version_header,
                        base::CompareCase::SENSITIVE)) {
    return false;
  }

  // Strip the header from |ruleset_data|.
  ruleset_data->erase(0, version_header.size());
  return true;
}

bool PersistIndexedRuleset(const base::FilePath& path,
                           base::span<const uint8_t> data,
                           int* ruleset_checksum) {
  DCHECK(ruleset_checksum);

  // Create the directory corresponding to |path| if it does not exist.
  if (!base::CreateDirectory(path.DirName()))
    return false;

  base::File ruleset_file(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!ruleset_file.IsValid())
    return false;

  // Write the version header.
  std::string version_header = GetVersionHeader();
  int version_header_size = static_cast<int>(version_header.size());
  if (ruleset_file.WriteAtCurrentPos(
          version_header.data(), version_header_size) != version_header_size) {
    return false;
  }

  // Write the flatbuffer ruleset.
  if (!base::IsValueInRangeForNumericType<int>(data.size()))
    return false;
  int data_size = static_cast<int>(data.size());
  if (ruleset_file.WriteAtCurrentPos(reinterpret_cast<const char*>(data.data()),
                                     data_size) != data_size) {
    return false;
  }

  *ruleset_checksum = GetChecksum(data);
  return true;
}

// Helper to clear each renderer's in-memory cache the next time it navigates.
void ClearRendererCacheOnNavigation() {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    ClearRendererCacheOnUI();
  } else {
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&ClearRendererCacheOnUI));
  }
}

void LogReadDynamicRulesStatus(ReadJSONRulesResult::Status status) {
  base::UmaHistogramEnumeration(kReadDynamicRulesJSONStatusHistogram, status);
}

// Maps content::ResourceType to api::declarative_net_request::ResourceType.
dnr_api::ResourceType GetDNRResourceType(content::ResourceType resource_type) {
  switch (resource_type) {
    case content::ResourceType::kPrefetch:
    case content::ResourceType::kSubResource:
      return dnr_api::RESOURCE_TYPE_OTHER;
    case content::ResourceType::kMainFrame:
    case content::ResourceType::kNavigationPreloadMainFrame:
      return dnr_api::RESOURCE_TYPE_MAIN_FRAME;
    case content::ResourceType::kCspReport:
      return dnr_api::RESOURCE_TYPE_CSP_REPORT;
    case content::ResourceType::kScript:
    case content::ResourceType::kWorker:
    case content::ResourceType::kSharedWorker:
    case content::ResourceType::kServiceWorker:
      return dnr_api::RESOURCE_TYPE_SCRIPT;
    case content::ResourceType::kImage:
    case content::ResourceType::kFavicon:
      return dnr_api::RESOURCE_TYPE_IMAGE;
    case content::ResourceType::kStylesheet:
      return dnr_api::RESOURCE_TYPE_STYLESHEET;
    case content::ResourceType::kObject:
    case content::ResourceType::kPluginResource:
      return dnr_api::RESOURCE_TYPE_OBJECT;
    case content::ResourceType::kXhr:
      return dnr_api::RESOURCE_TYPE_XMLHTTPREQUEST;
    case content::ResourceType::kSubFrame:
    case content::ResourceType::kNavigationPreloadSubFrame:
      return dnr_api::RESOURCE_TYPE_SUB_FRAME;
    case content::ResourceType::kPing:
      return dnr_api::RESOURCE_TYPE_PING;
    case content::ResourceType::kMedia:
      return dnr_api::RESOURCE_TYPE_MEDIA;
    case content::ResourceType::kFontResource:
      return dnr_api::RESOURCE_TYPE_FONT;
  }
  NOTREACHED();
  return dnr_api::RESOURCE_TYPE_OTHER;
}

dnr_api::RequestDetails CreateRequestDetails(const WebRequestInfo& request) {
  api::declarative_net_request::RequestDetails details;
  details.request_id = base::NumberToString(request.id);
  details.url = request.url.spec();

  if (request.initiator) {
    details.initiator =
        std::make_unique<std::string>(request.initiator->Serialize());
  }

  details.method = request.method;
  details.frame_id = request.frame_data.frame_id;
  details.parent_frame_id = request.frame_data.parent_frame_id;
  details.tab_id = request.frame_data.tab_id;
  details.type = GetDNRResourceType(request.type);
  return details;
}

}  // namespace declarative_net_request
}  // namespace extensions
