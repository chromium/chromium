// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_FONT_DATA_FONT_DATA_MANAGER_H_
#define CONTENT_CHILD_FONT_DATA_FONT_DATA_MANAGER_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/containers/lru_cache.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/unguessable_token.h"
#include "components/services/font_data/public/mojom/font_data_service.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/freetype_buildflags.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace base {
class MemoryMappedFile;
}

namespace font_data_service {

// Replaces the default font manager in the child.
// Does the following:
//     - Instantiate child -> browser connection.
//     - Request font from the browser based on family name and font style.
//     - Convert shared memory region to a SkTypeface and cache the results.
//
// Instantiated in the child on startup. Font fallback mechanism is still in
// place/has not changed.
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

  size_t GetMappedFilesCountForTesting() const;

 private:
  font_data_service::mojom::FontDataService& GetRemoteFontDataService() const;

  sk_sp<SkTypeface> CreateTypefaceFromMatchResult(
      mojom::MatchFamilyNameResultPtr match_result) const;

  // This must be const to allow being called from onCountFamilies and
  // onGetFamilyNames, but it does mutate family_names_. Requires holding
  // `family_names_lock_`.
  void GetAllFamilyNamesLockRequired() const;

  // Key of the typeface_cache_.
  struct MatchFamilyRequest {
    MatchFamilyRequest(std::optional<std::string> name,
                       int weight,
                       int width,
                       SkFontStyle::Slant slant);
    MatchFamilyRequest(const MatchFamilyRequest&);
    MatchFamilyRequest(MatchFamilyRequest&&);
    ~MatchFamilyRequest();
    std::optional<std::string> name;
    int weight;
    int width;
    SkFontStyle::Slant slant;
  };

  // Tries to get the matching typeface from the cache if it exists, returns
  // nullopt otherwise.
  std::optional<sk_sp<SkTypeface>> TryGetFromCache(
      const MatchFamilyRequest& request) const;

  // Adds the specified typeface to the cache for the key represented by the
  // match request params.
  void AddToCache(const MatchFamilyRequest& request,
                  sk_sp<SkTypeface> typeface) const;

  struct MatchFamilyRequestHash {
    size_t operator()(const MatchFamilyRequest& key) const {
      return base::HashCombine(0ull, key.name, key.weight, key.width,
                               key.slant);
    }
  };
  struct MatchFamilyRequestEqual {
    size_t operator()(const MatchFamilyRequest& lhs,
                      const MatchFamilyRequest& rhs) const {
      return lhs.name == rhs.name && lhs.weight == rhs.weight &&
             lhs.width == rhs.width && lhs.slant == rhs.slant;
    }
  };

  // Calls to this class can be on any thread hence there is a lock to guard
  // each of the caches. The mapped regions, mapped files, typeface, and family
  // names caches are independent of one another (they're not read/written in
  // the same circumstances and they don't have to stay in sync), so they can
  // each get a separate lock.
  mutable base::Lock mapped_regions_lock_;
  mutable base::Lock mapped_files_lock_;
  mutable base::Lock family_names_lock_;
  mutable base::Lock typeface_cache_lock_;

  // Cache of the font requests to existing typefaces. Allows replying directly
  // with a typeface for a match request that was already made.
  mutable base::HashingLRUCache<MatchFamilyRequest,
                                sk_sp<SkTypeface>,
                                MatchFamilyRequestHash,
                                MatchFamilyRequestEqual>
      typeface_cache_ GUARDED_BY(typeface_cache_lock_);

  // Cache of the shared memory region by GUID to known font mappings. Allows
  // reusing a pre-existing shared memory region if it contains the data
  // required to fulfill a match request even though that match request doesn't
  // correspond to a typeface cached in `typeface_cache_`.
  mutable absl::flat_hash_map<base::UnguessableToken,
                              base::ReadOnlySharedMemoryMapping>
      mapped_regions_ GUARDED_BY(mapped_regions_lock_);

  // Cache of the memory mapped files to ensure the mapping lives.
  // The key is an ID received from the child along with the handle that
  // uniquely identifies the font file. If 2 matching requests resolve to fonts
  // that are contained within the same file, the child would receive 2
  // different handles to the underlying file but each of those requests would
  // receive the same ID. This is used to ensure we reuse an already mapped
  // file if possible rather than creating a new one for each match that
  // resolves to the same file on disk. It's crucial that a mapped file from
  // this map is never unmapped or replaced, as we have already given out
  // typeface objects that reference them which can be used for the entire
  // lifetime of the child process.
  mutable absl::flat_hash_map<uint64_t, std::unique_ptr<base::MemoryMappedFile>>
      mapped_files_ GUARDED_BY(mapped_files_lock_);

  // A cache of all the font family names that could be returned by
  // onGetFamilyName. When populated, this has the same amount of elements as
  // returned by onCountFamilies. This is populated on the first call to either
  // onCountFamilies or onGetFamilyName.
  mutable std::vector<std::string> family_names_ GUARDED_BY(family_names_lock_);

#if BUILDFLAG(ENABLE_FREETYPE)
  sk_sp<SkFontMgr> custom_fnt_mgr_;
#endif
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  mutable base::SequenceLocalStorageSlot<
      mojo::Remote<font_data_service::mojom::FontDataService>>
      font_data_service_slot_;
};

}  // namespace font_data_service

#endif  // CONTENT_CHILD_FONT_DATA_FONT_DATA_MANAGER_H_
