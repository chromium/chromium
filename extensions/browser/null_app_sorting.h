// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_NULL_APP_SORTING_H_
#define EXTENSIONS_BROWSER_NULL_APP_SORTING_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// An AppSorting that doesn't provide any ordering.
class NullAppSorting : public AppSorting {
 public:
  NullAppSorting();

  NullAppSorting(const NullAppSorting&) = delete;
  NullAppSorting& operator=(const NullAppSorting&) = delete;

  ~NullAppSorting() override;

  // AppSorting overrides:
  void InitializePageOrdinalMapFromWebApps() override;
  void FixNTPOrdinalCollisions() override;
  void EnsureValidOrdinals(
      const ExtensionId& extension_id,
      const syncer::StringOrdinal& suggested_page) override;
  bool GetDefaultOrdinals(const ExtensionId& extension_id,
                          syncer::StringOrdinal* page_ordinal,
                          syncer::StringOrdinal* app_launch_ordinal) override;
  void OnExtensionMoved(const ExtensionId& moved_extension_id,
                        const ExtensionId& predecessor_extension_id,
                        const ExtensionId& successor_extension_id) override;
  syncer::StringOrdinal GetAppLaunchOrdinal(
      const ExtensionId& extension_id) const override;
  void SetAppLaunchOrdinal(
      const ExtensionId& extension_id,
      const syncer::StringOrdinal& new_app_launch_ordinal) override;
  syncer::StringOrdinal CreateFirstAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const override;
  syncer::StringOrdinal CreateNextAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const override;
  syncer::StringOrdinal CreateFirstAppPageOrdinal() const override;
  syncer::StringOrdinal GetNaturalAppPageOrdinal() const override;
  syncer::StringOrdinal GetPageOrdinal(
      const ExtensionId& extension_id) const override;
  void SetPageOrdinal(const ExtensionId& extension_id,
                      const syncer::StringOrdinal& new_page_ordinal) override;
  void ClearOrdinals(const ExtensionId& extension_id) override;
  int PageStringOrdinalAsInteger(
      const syncer::StringOrdinal& page_ordinal) const override;
  syncer::StringOrdinal PageIntegerAsStringOrdinal(size_t page_index) override;
  void SetExtensionVisible(const ExtensionId& extension_id,
                           bool visible) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_NULL_APP_SORTING_H_
