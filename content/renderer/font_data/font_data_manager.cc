// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/font_data/font_data_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/skia/src/ports/SkTypeface_win_dw.h"  // nogncheck
#if BUILDFLAG(ENABLE_FREETYPE)
#include "third_party/skia/include/ports/SkFontMgr_empty.h"
#endif
#include "third_party/skia/include/ports/SkTypeface_fontations.h"

namespace font_data_service {

namespace {

const int kTypefaceCacheSize = 128;

// Binds a pending receiver. Must be invoked from the main thread.
void BindHostReceiverOnMainThread(
    mojo::PendingReceiver<font_data_service::mojom::FontDataService>
        pending_receiver) {
  content::ChildThread::Get()->BindHostReceiver(std::move(pending_receiver));
}

mojom::TypefaceSlant ConvertToMojomFontStyle(SkFontStyle::Slant slant) {
  switch (slant) {
    case SkFontStyle::Slant::kUpright_Slant:
      return mojom::TypefaceSlant::kRoman;
    case SkFontStyle::Slant::kItalic_Slant:
      return mojom::TypefaceSlant::kItalic;
    case SkFontStyle::Slant::kOblique_Slant:
      return mojom::TypefaceSlant::kOblique;
  }
  NOTREACHED();
}

UNSAFE_BUFFER_USAGE std::vector<std::string> bcp47ArrayToVector(
    const char* bcp47_array[],
    int bcp47_count) {
  std::vector<std::string> bcp47s;
  bcp47s.reserve(bcp47_count);

  // SAFETY: Skia passes BCP47 language tags as an array of `bcp47_count`
  // null-terminated c-style strings. Generate an equivalent
  // std::vector<std::string> to pass over mojo.
  for (int i = 0; i < bcp47_count; ++i) {
    bcp47s.emplace_back(UNSAFE_BUFFERS(bcp47_array[i]));
  }

  return bcp47s;
}
}  // namespace

FontDataManager::FontDataManager()
    : typeface_cache_(kTypefaceCacheSize),
#if BUILDFLAG(ENABLE_FREETYPE)
      custom_fnt_mgr_(SkFontMgr_New_Custom_Empty()),
#endif
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  CHECK(content::RenderThread::IsMainThread());
}

FontDataManager::~FontDataManager() = default;

int FontDataManager::onCountFamilies() const {
  if (family_names_.empty()) {
    GetAllFamilyNames();
  }

  return family_names_.size();
}

void FontDataManager::onGetFamilyName(int index,
                                      SkString* requested_family_name) const {
  if (family_names_.empty()) {
    GetAllFamilyNames();
  }

  if (index < 0) {
    return;
  }

  size_t family_index = static_cast<size_t>(index);
  if (family_index >= family_names_.size()) {
    return;
  }

  *requested_family_name = SkString(family_names_[family_index]);
}

sk_sp<SkFontStyleSet> FontDataManager::onCreateStyleSet(int index) const {
  NOTREACHED();
}

sk_sp<SkFontStyleSet> FontDataManager::onMatchFamily(
    const char requested_family_name[]) const {
  NOTREACHED();
}

sk_sp<SkTypeface> FontDataManager::onMatchFamilyStyle(
    const char requested_family_name[],
    const SkFontStyle& requested_style) const {
  // NOTE: requested_family_name can be null to get default font.
  std::string cpp_requested_family_name;
  if (requested_family_name) {
    cpp_requested_family_name = requested_family_name;
  }

  MatchFamilyRequest match_request{.name = cpp_requested_family_name,
                                   .weight = requested_style.weight(),
                                   .width = requested_style.width(),
                                   .slant = requested_style.slant()};
  {
    base::AutoLock locked(lock_);
    auto iter = typeface_cache_.Get(match_request);
    if (iter != typeface_cache_.end()) {
      return iter->second;
    }
  }
  // Proxy the font request to the font service.
  mojom::TypefaceStylePtr style(mojom::TypefaceStyle::New());
  style->weight = requested_style.weight();
  style->width = requested_style.width();
  style->slant = ConvertToMojomFontStyle(requested_style.slant());

  mojom::MatchFamilyNameResultPtr match_result;
  {
    TRACE_EVENT1("fonts", "FontDataManager::onMatchFamilyStyle", "family_name",
                 cpp_requested_family_name);
    GetRemoteFontDataService().MatchFamilyName(cpp_requested_family_name,
                                               std::move(style), &match_result);
  }

  auto typeface = CreateTypefaceFromMatchResult(std::move(match_result));

  // Update the cache with the resulting typeface even in case of a failure to
  // avoid calling the font service again. Failed typeface will go to the font
  // fallback stack.
  {
    base::AutoLock locked(lock_);
    typeface_cache_.Put(std::move(match_request), typeface);
  }

  return typeface;
}

sk_sp<SkTypeface> FontDataManager::onMatchFamilyStyleCharacter(
    const char requested_family_name[],
    const SkFontStyle& requested_style,
    const char* bcp47[],
    int bcp47_count,
    SkUnichar character) const {
  // NOTE: requested_family_name can be null to get default font.
  std::string cpp_requested_family_name;
  if (requested_family_name) {
    cpp_requested_family_name = requested_family_name;
  }

  MatchFamilyRequest match_request{.name = cpp_requested_family_name,
                                   .weight = requested_style.weight(),
                                   .width = requested_style.width(),
                                   .slant = requested_style.slant()};

  // Proxy the font request to the font service.
  mojom::TypefaceStylePtr style(mojom::TypefaceStyle::New());
  style->weight = requested_style.weight();
  style->width = requested_style.width();
  style->slant = ConvertToMojomFontStyle(requested_style.slant());

  mojom::MatchFamilyNameResultPtr match_result;
  {
    TRACE_EVENT1("fonts", "FontDataManager::onMatchFamilyStyleCharacter",
                 "family_name", cpp_requested_family_name);

    // SAFETY: Skia passes BCP47 language tags as an array of `bcp47_count`
    // null-terminated c-style strings. Generate an equivalent
    // std::vector<std::string> to pass over mojo.
    GetRemoteFontDataService().MatchFamilyNameCharacter(
        cpp_requested_family_name, std::move(style),
        UNSAFE_BUFFERS(bcp47ArrayToVector(bcp47, bcp47_count)), character,
        &match_result);
  }

  return CreateTypefaceFromMatchResult(std::move(match_result));
}

sk_sp<SkTypeface> FontDataManager::onMakeFromData(sk_sp<SkData> data,
                                                  int ttc_index) const {
  return makeFromStream(std::make_unique<SkMemoryStream>(std::move(data)),
                        ttc_index);
}

sk_sp<SkTypeface> FontDataManager::onMakeFromStreamIndex(
    std::unique_ptr<SkStreamAsset> stream,
    int ttc_index) const {
  SkFontArguments args;
  args.setCollectionIndex(ttc_index);
  return onMakeFromStreamArgs(std::move(stream), args);
}

sk_sp<SkTypeface> FontDataManager::onMakeFromStreamArgs(
    std::unique_ptr<SkStreamAsset> stream,
    const SkFontArguments& args) const {
  TRACE_EVENT1("fonts", "FontDataManager::onMakeFromStreamArgs", "size",
               stream->getLength());
  // Experiment will test the performance of different SkTypefaces.
  // 'custom_fnt_mgr_' is a wrapper to create an SkFreeType typeface.

  if (features::kFontDataServiceTypefaceType.Get() ==
      features::FontDataServiceTypefaceType::kDwrite) {
    return DWriteFontTypeface::MakeFromStream(std::move(stream), args);
  } else if (features::kFontDataServiceTypefaceType.Get() ==
             features::FontDataServiceTypefaceType::kFreetype) {
    // Chromium currently always sets ENABLE_FREETYPE, but nonetheless allow
    // falling back to fontations if the param is set to freetype but freetype
    // isn't enabled.
#if BUILDFLAG(ENABLE_FREETYPE)
    return custom_fnt_mgr_->makeFromStream(std::move(stream), args);
#endif
  }

  return SkTypeface_Make_Fontations(std::move(stream), args);
}

sk_sp<SkTypeface> FontDataManager::onMakeFromFile(const char path[],
                                                  int ttc_index) const {
  std::unique_ptr<SkStreamAsset> stream = SkStream::MakeFromFile(path);
  return stream ? makeFromStream(std::move(stream), ttc_index) : nullptr;
}

sk_sp<SkTypeface> FontDataManager::onLegacyMakeTypeface(
    const char requested_family_name[],
    SkFontStyle requested_style) const {
  std::optional<std::string> cpp_requested_family_name;
  if (requested_family_name) {
    cpp_requested_family_name = requested_family_name;
  }

  // Proxy the font request to the font service.
  mojom::TypefaceStylePtr style(mojom::TypefaceStyle::New());
  style->weight = requested_style.weight();
  style->width = requested_style.width();
  style->slant = ConvertToMojomFontStyle(requested_style.slant());

  mojom::MatchFamilyNameResultPtr match_result;
  {
    TRACE_EVENT1("fonts", "FontDataManager::onLegacyMakeTypeface",
                 "family_name", cpp_requested_family_name);

    GetRemoteFontDataService().LegacyMakeTypeface(
        cpp_requested_family_name, std::move(style), &match_result);
  }

  return CreateTypefaceFromMatchResult(std::move(match_result));
}

void FontDataManager::SetFontServiceForTesting(
    mojo::PendingRemote<font_data_service::mojom::FontDataService>
        font_data_service) {
  mojo::Remote<font_data_service::mojom::FontDataService>& remote =
      font_data_service_slot_.GetOrCreateValue();
  remote.Bind(std::move(font_data_service));
}

font_data_service::mojom::FontDataService&
FontDataManager::GetRemoteFontDataService() const {
  mojo::Remote<font_data_service::mojom::FontDataService>& remote =
      font_data_service_slot_.GetOrCreateValue();

  if (!remote) {
    if (main_task_runner_->RunsTasksInCurrentSequence()) {
      BindHostReceiverOnMainThread(remote.BindNewPipeAndPassReceiver());
    } else {
      main_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&BindHostReceiverOnMainThread,
                                    remote.BindNewPipeAndPassReceiver()));
    }
  }
  return *remote;
}

sk_sp<SkTypeface> FontDataManager::CreateTypefaceFromMatchResult(
    mojom::MatchFamilyNameResultPtr match_result) const {
  // Create the resulting typeface from the data received from the font
  // service.
  std::unique_ptr<base::MemoryMappedFile> mapped_font_file;
  sk_sp<SkTypeface> typeface;
  if (match_result && match_result->typeface_data) {
    // Attempt to create the SkFontArguments args.
    base::HeapArray<SkFontArguments::VariationPosition::Coordinate>
        typeface_axis;
    SkFontArguments args;
    args.setCollectionIndex(match_result->ttc_index);
    if (match_result->variation_position &&
        match_result->variation_position->coordinateCount > 0) {
      typeface_axis =
          base::HeapArray<SkFontArguments::VariationPosition::Coordinate>::
              Uninit(match_result->variation_position->coordinates.size());
      for (size_t i = 0;
           i < match_result->variation_position->coordinates.size(); i++) {
        const auto& coordinate =
            match_result->variation_position->coordinates[i];
        typeface_axis[i] = {.axis = coordinate->axis,
                            .value = coordinate->value};
      }
      args.setVariationDesignPosition(
          {.coordinates = typeface_axis.data(),
           .coordinateCount = base::checked_cast<int>(typeface_axis.size())});
    }

    // Attempt to create the typeface data based on the match result.
    if (match_result->typeface_data->is_font_file()) {
      TRACE_EVENT("fonts", "FontDataManager - using mapped file");
      if (auto file_mapping = std::make_unique<base::MemoryMappedFile>();
          file_mapping->Initialize(
              std::move(match_result->typeface_data->get_font_file()))) {
        typeface = onMakeFromStreamArgs(
            SkMemoryStream::MakeDirect(file_mapping->data(),
                                       file_mapping->length()),
            args);
        if (typeface) {
          // The typeface was found, so keep the mapping alive.
          mapped_font_file = std::move(file_mapping);
        }
      }
    } else if (match_result->typeface_data->is_region() &&
               match_result->typeface_data->get_region().IsValid()) {
      TRACE_EVENT("fonts", "FontDataManager - using shared memory");
      base::ReadOnlySharedMemoryRegion font_data_memory_region =
          std::move(match_result->typeface_data->get_region());
      const void* mapped_memory = nullptr;
      size_t mapped_size = 0;
      // Map the memory (if needed) and keep it alive for the lifetime of this
      // process. A cache is used to avoid mapping the same memory space
      // multiple time.
      {
        base::AutoLock locked(lock_);
        const auto iter =
            mapped_regions_.lower_bound(font_data_memory_region.GetGUID());
        if (iter != mapped_regions_.end() &&
            iter->first == font_data_memory_region.GetGUID()) {
          mapped_memory = iter->second.memory();
          mapped_size = iter->second.size();
        } else {
          base::ReadOnlySharedMemoryMapping mapping =
              font_data_memory_region.Map();
          if (mapping.IsValid()) {
            mapped_memory = mapping.memory();
            mapped_size = mapping.size();
            mapped_regions_.insert_or_assign(
                iter, font_data_memory_region.GetGUID(), std::move(mapping));
          }
        }
      }

      // Create the memory stream from the mapped memory.
      if (mapped_memory && mapped_size > 0) {
        typeface = onMakeFromStreamArgs(
            SkMemoryStream::MakeDirect(mapped_memory, mapped_size), args);
      }
    }
  }

  // Add the mapped_font_file to the cache so it lives as long as required.
  {
    base::AutoLock locked(lock_);
    if (mapped_font_file) {
      mapped_files_.push_back(std::move(mapped_font_file));
    }
  }

  return typeface;
}

void FontDataManager::GetAllFamilyNames() const {
  GetRemoteFontDataService().GetAllFamilyNames(&family_names_);
}

}  // namespace font_data_service
