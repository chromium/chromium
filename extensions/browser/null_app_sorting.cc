// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/null_app_sorting.h"

#include "components/sync/model/string_ordinal.h"

namespace {

// Ordinals for a single app on a single page.
const char kFirstApp[] = "a";
const char kNextApp[] = "b";
const char kFirstPage[] = "a";

}  // namespace

namespace extensions {

NullAppSorting::NullAppSorting() {
}

NullAppSorting::~NullAppSorting() {
}

void NullAppSorting::InitializePageOrdinalMapFromWebApps() {}

void NullAppSorting::FixNTPOrdinalCollisions() {
}

void NullAppSorting::EnsureValidOrdinals(
    const std::string& extension_id,
    const syncer::StringOrdinal& suggested_page) {
}

bool NullAppSorting::GetDefaultOrdinals(
    const std::string& extension_id,
    syncer::StringOrdinal* page_ordinal,
    syncer::StringOrdinal* app_launch_ordinal) {
  return false;
}

void NullAppSorting::OnExtensionMoved(
    const std::string& moved_extension_id,
    const std::string& predecessor_extension_id,
    const std::string& successor_extension_id) {
}

syncer::StringOrdinal NullAppSorting::GetAppLaunchOrdinal(
    const std::string& extension_id) const {
  return syncer::StringOrdinal(kFirstApp);
}

void NullAppSorting::SetAppLaunchOrdinal(
    const std::string& extension_id,
    const syncer::StringOrdinal& new_app_launch_ordinal) {
}

syncer::StringOrdinal NullAppSorting::CreateFirstAppLaunchOrdinal(
    const syncer::StringOrdinal& page_ordinal) const {
  return syncer::StringOrdinal(kFirstApp);
}

syncer::StringOrdinal NullAppSorting::CreateNextAppLaunchOrdinal(
    const syncer::StringOrdinal& page_ordinal) const {
  return syncer::StringOrdinal(kNextApp);
}

syncer::StringOrdinal NullAppSorting::CreateFirstAppPageOrdinal() const {
  return syncer::StringOrdinal(kFirstPage);
}

syncer::StringOrdinal NullAppSorting::GetNaturalAppPageOrdinal() const {
  return syncer::StringOrdinal(kFirstPage);
}

syncer::StringOrdinal NullAppSorting::GetPageOrdinal(
    const std::string& extension_id) const {
  return syncer::StringOrdinal(kFirstPage);
}

void NullAppSorting::SetPageOrdinal(
    const std::string& extension_id,
    const syncer::StringOrdinal& new_page_ordinal) {
}

void NullAppSorting::ClearOrdinals(const std::string& extension_id) {
}

int NullAppSorting::PageStringOrdinalAsInteger(
    const syncer::StringOrdinal& page_ordinal) const {
  return 0;
}

syncer::StringOrdinal NullAppSorting::PageIntegerAsStringOrdinal(
    size_t page_index) {
  return syncer::StringOrdinal(kFirstPage);
}

void NullAppSorting::SetExtensionVisible(const std::string& extension_id,
                                         bool visible) {
}

}  // namespace extensions
