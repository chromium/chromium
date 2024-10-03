// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_FONT_DATA_FONT_DATA_MANAGER_H_
#define CONTENT_RENDERER_FONT_DATA_FONT_DATA_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/lru_cache.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/unguessable_token.h"
#include "components/services/font_data/public/mojom/font_data_service.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/freetype_buildflags.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace font_data_service {

// Replaces the default font manager in the renderer.
// Does the following:
//     - Instantiate renderer -> browser connection.
//     - Request font from the browser based on family name and font style.
//     - Convert shared memory region to a SkTypeface and cache the results.
//
// Instantiated in the renderer on startup. Font fallback mechanism is still in
// place/has not changed. Only runs on renderers for TopChrome WebUI on Windows
// as part of an experiment: see crbug.com/335680565 for more details.
//
// The methods of this class (as imposed by blink requirements) may be called on
// any thread.
class CONTENT_EXPORT FontDataManager : public SkFontMgr {
 public:
  FontDataManager();
  FontDataManager(const FontDataManager&) = delete;
  FontDataManager& operator=(const FontDataManager&) = delete;
  ~FontDataManager() override;

  // SkFontMgr:
  int onCountFamilies() const override;
  void onGetFamilyName(int index, SkString* familyName) const override;
  sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override;
  /** May return NULL if the name is not found. */
  sk_sp<SkFontStyleSet> onMatchFamily(const char familyName[]) const override;
  sk_sp<SkTypeface> onMatchFamilyStyle(const char familyName[],
                                       const SkFontStyle&) const override;
  sk_sp<SkTypeface> onMatchFamilyStyleCharacter(
      const char familyName[],
      const SkFontStyle&,
      const char* bcp47[],
      int bcp47Count,
      SkUnichar character) const override;
  sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData>, int ttcIndex) const override;
  sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset>,
                                          int ttcIndex) const override;
  sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset>,
                                         const SkFontArguments&) const override;
  sk_sp<SkTypeface> onMakeFromFile(const char path[],
                                   int ttcIndex) const override;
  sk_sp<SkTypeface> onLegacyMakeTypeface(const char familyName[],
                                         SkFontStyle) const override;

  void SetFontServiceForTesting(
      mojo::PendingRemote<font_data_service::mojom::FontDataService>
          font_data_service);

 private:
  font_data_service::mojom::FontDataService& GetRemoteFontDataService() const;

  // Key of the typeface_cache_.
  struct MatchFamilyRequest {
    std::string name;
    int weight;
    int width;
    SkFontStyle::Slant slant;
  };
  struct MatchFamilyRequestHash {
    size_t operator()(const MatchFamilyRequest& key) const {
      return std::hash<std::string>{}(key.name) ^ key.weight ^
             (key.width << 8) ^ (key.slant << 16);
    }
  };
  struct MatchFamilyRequestEqual {
    size_t operator()(const MatchFamilyRequest& lhs,
                      const MatchFamilyRequest& rhs) const {
      return lhs.name == rhs.name && lhs.weight == rhs.weight &&
             lhs.width == rhs.width && lhs.slant == rhs.slant;
    }
  };
  // Cache of the font requests to existing typefaces.
  mutable base::HashingLRUCache<MatchFamilyRequest,
                                sk_sp<SkTypeface>,
                                MatchFamilyRequestHash,
                                MatchFamilyRequestEqual>
      typeface_cache_;

  // Calls to this class can be on any thread hence there is a lock to guard
  // the cache.
  mutable base::Lock lock_;

  // Cache of the shared memory region by GUID to known font mappings.
  mutable std::map<base::UnguessableToken, base::ReadOnlySharedMemoryMapping>
      mapped_regions_;

#if BUILDFLAG(ENABLE_FREETYPE)
  sk_sp<SkFontMgr> custom_fnt_mgr_;
#endif
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  mutable base::SequenceLocalStorageSlot<
      mojo::Remote<font_data_service::mojom::FontDataService>>
      font_data_service_slot_;
};

}  // namespace font_data_service

#endif  // CONTENT_RENDERER_FONT_DATA_FONT_DATA_MANAGER_H_
