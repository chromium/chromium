// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/test_shared_storage_observer.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "content/browser/shared_storage/shared_storage_event_params.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {
using SharedStorageUrlSpecWithMetadata =
    SharedStorageEventParams::SharedStorageUrlSpecWithMetadata;

std::string SerializeOptionalString(std::optional<std::string> str) {
  if (str) {
    return *str;
  }

  return "std::nullopt";
}

std::string SerializeOptionalBool(std::optional<bool> b) {
  if (b) {
    return base::ToString(*b);
  }

  return "std::nullopt";
}

std::string SerializeOptionalUrlsWithMetadata(
    std::optional<std::vector<SharedStorageUrlSpecWithMetadata>>
        urls_with_metadata) {
  if (!urls_with_metadata) {
    return "std::nullopt";
  }

  std::vector<std::string> urls_str_vector = {"{ "};
  for (const auto& url_with_metadata : *urls_with_metadata) {
    urls_str_vector.push_back("{url: ");
    urls_str_vector.push_back(url_with_metadata.url);
    urls_str_vector.push_back(", reporting_metadata: { ");
    for (const auto& metadata_pair : url_with_metadata.reporting_metadata) {
      urls_str_vector.push_back("{");
      urls_str_vector.push_back(metadata_pair.first);
      urls_str_vector.push_back(" : ");
      urls_str_vector.push_back(metadata_pair.second);
      urls_str_vector.push_back("} ");
    }
    urls_str_vector.push_back("}} ");
  }
  urls_str_vector.push_back("}");

  return base::StrCat(urls_str_vector);
}
}  // namespace

TestSharedStorageObserver::TestSharedStorageObserver() = default;
TestSharedStorageObserver::~TestSharedStorageObserver() = default;

void TestSharedStorageObserver::OnSharedStorageAccessed(
    const base::Time& access_time,
    AccessType type,
    FrameTreeNodeId main_frame_id,
    const std::string& owner_origin,
    const SharedStorageEventParams& params) {
  accesses_.emplace_back(type, main_frame_id, owner_origin, params);
}

void TestSharedStorageObserver::OnUrnUuidGenerated(const GURL& urn_uuid) {}

void TestSharedStorageObserver::OnConfigPopulated(
    const std::optional<FencedFrameConfig>& config) {}

bool TestSharedStorageObserver::EventParamsMatch(
    const SharedStorageEventParams& expected_params,
    const SharedStorageEventParams& actual_params) {
  if (expected_params.script_source_url != actual_params.script_source_url) {
    LOG(ERROR) << "expected `script_source_url`: '"
               << SerializeOptionalString(expected_params.script_source_url)
               << "'";
    LOG(ERROR) << "actual `sript_source_url`:   '"
               << SerializeOptionalString(actual_params.script_source_url)
               << "'";
    return false;
  }
  if (expected_params.operation_name != actual_params.operation_name) {
    LOG(ERROR) << "expected `operation_name`: '"
               << SerializeOptionalString(expected_params.operation_name)
               << "'";
    LOG(ERROR) << "actual `operation_name`:   '"
               << SerializeOptionalString(actual_params.operation_name) << "'";
    return false;
  }
  if (expected_params.urls_with_metadata != actual_params.urls_with_metadata) {
    LOG(ERROR) << "expected `urls_with_metadata`: "
               << SerializeOptionalUrlsWithMetadata(
                      expected_params.urls_with_metadata);
    LOG(ERROR) << "actual `urls_with_metadata`:   "
               << SerializeOptionalUrlsWithMetadata(
                      actual_params.urls_with_metadata);
    return false;
  }
  if (expected_params.key != actual_params.key) {
    LOG(ERROR) << "expected `key`: '"
               << SerializeOptionalString(expected_params.key) << "'";
    LOG(ERROR) << "actual key:   '"
               << SerializeOptionalString(actual_params.key) << "'";
    return false;
  }
  if (expected_params.value != actual_params.value) {
    LOG(ERROR) << "expected `value`: '"
               << SerializeOptionalString(expected_params.value) << "'";
    LOG(ERROR) << "actual `value`:   '"
               << SerializeOptionalString(actual_params.value) << "'";
    return false;
  }
  if (expected_params.ignore_if_present != actual_params.ignore_if_present) {
    LOG(ERROR) << "expected `ignore_if_present`: "
               << SerializeOptionalBool(expected_params.ignore_if_present);
    LOG(ERROR) << "actual `ignore_if_present`:   "
               << SerializeOptionalBool(actual_params.ignore_if_present);
    return false;
  }

  if (expected_params.serialized_data && !actual_params.serialized_data) {
    LOG(ERROR) << "`serialized_data` unexpectedly null";
    LOG(ERROR) << "expected `serialized_data`: '"
               << SerializeOptionalString(expected_params.serialized_data)
               << "'";
    LOG(ERROR) << "actual `serialized_data`: '"
               << SerializeOptionalString(actual_params.serialized_data) << "'";
    return false;
  }

  if (!expected_params.serialized_data && actual_params.serialized_data) {
    LOG(ERROR) << "`serialized_data` unexpectedly non-null";
    LOG(ERROR) << "expected `serialized_data`: '"
               << SerializeOptionalString(expected_params.serialized_data)
               << "'";
    LOG(ERROR) << "actual `serialized_data`: '"
               << SerializeOptionalString(actual_params.serialized_data) << "'";
    return false;
  }

  return true;
}

bool TestSharedStorageObserver::AccessesMatch(const Access& expected_access,
                                              const Access& actual_access) {
  if (std::get<0>(expected_access) != std::get<0>(actual_access)) {
    LOG(ERROR) << "expected `type`: " << std::get<0>(expected_access);
    LOG(ERROR) << "actual `type`:   " << std::get<0>(actual_access);
    return false;
  }

  if (std::get<1>(expected_access) != std::get<1>(actual_access)) {
    LOG(ERROR) << "expected `main_frame_id`: '" << std::get<1>(expected_access)
               << "'";
    LOG(ERROR) << "actual `main_frame_id`:   '" << std::get<1>(actual_access)
               << "'";
    return false;
  }

  if (std::get<2>(expected_access) != std::get<2>(actual_access)) {
    LOG(ERROR) << "expected `origin`: '" << std::get<2>(expected_access) << "'";
    LOG(ERROR) << "actual `origin`:   '" << std::get<2>(actual_access) << "'";
    return false;
  }

  return EventParamsMatch(std::get<3>(expected_access),
                          std::get<3>(actual_access));
}

void TestSharedStorageObserver::ExpectAccessObserved(
    const std::vector<Access>& expected_accesses) {
  ASSERT_EQ(expected_accesses.size(), accesses_.size());
  for (size_t i = 0; i < accesses_.size(); ++i) {
    EXPECT_TRUE(AccessesMatch(expected_accesses[i], accesses_[i]));
    if (!AccessesMatch(expected_accesses[i], accesses_[i])) {
      LOG(ERROR) << "Event access at index " << i << " differs";
    }
  }
}

}  // namespace content
