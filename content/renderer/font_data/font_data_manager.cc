// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/font_data/font_data_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

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
#else
#include "third_party/skia/include/ports/SkTypeface_fontations.h"
#endif

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
  NOTREACHED();
}

void FontDataManager::onGetFamilyName(int index,
                                      SkString* requested_family_name) const {
  NOTREACHED();
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
    TRACE_EVENT1("fonts", "FontDataManager::onMakeFromStreamArgs - Remote Call",
                 "family_name", cpp_requested_family_name);
    GetRemoteFontDataService().MatchFamilyName(cpp_requested_family_name,
                                               std::move(style), &match_result);
  }

  // Create the resulting typeface from the bytes received from the font
  // service.
  sk_sp<SkTypeface> typeface;
  if (match_result) {
    base::ReadOnlySharedMemoryRegion font_data_memory_region =
        std::move(match_result->region);
    const void* mapped_memory = nullptr;
    size_t mapped_size = 0;
    // Map the memory (if needed) and keep it alive for the lifetime of this
    // process. A cache is used to avoid mapping the same memory space multiple
    // time.
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
      SkFontArguments args;
      args.setCollectionIndex(match_result->ttc_index);
      // Convert the variation position from mojom to an SkFontArguments struct.
      std::vector<SkFontArguments::VariationPosition::Coordinate> typeface_axis;
      if (match_result->variation_position &&
          match_result->variation_position->coordinateCount > 0) {
        typeface_axis.reserve(
            match_result->variation_position->coordinates.size());
        for (const auto& coordinate :
             match_result->variation_position->coordinates) {
          typeface_axis.push_back(
              {.axis = coordinate->axis, .value = coordinate->value});
        }
        args.setVariationDesignPosition(
            {.coordinates = typeface_axis.data(),
             .coordinateCount = base::checked_cast<int>(typeface_axis.size())});
      }
      typeface = onMakeFromStreamArgs(
          SkMemoryStream::MakeDirect(mapped_memory, mapped_size), args);
    }
  }

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
  NOTREACHED();
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

  return features::kSkiaFontServiceTypefaceType.Get() ==
                 features::SkiaFontServiceTypefaceType::kFreetype
             ?
#if BUILDFLAG(ENABLE_FREETYPE)
             custom_fnt_mgr_->makeFromStream(std::move(stream), args)
#else
             SkTypeface_Make_Fontations(std::move(stream), args)
#endif
             : DWriteFontTypeface::MakeFromStream(std::move(stream), args);
}

sk_sp<SkTypeface> FontDataManager::onMakeFromFile(const char path[],
                                                  int ttc_index) const {
  std::unique_ptr<SkStreamAsset> stream = SkStream::MakeFromFile(path);
  return stream ? makeFromStream(std::move(stream), ttc_index) : nullptr;
}

sk_sp<SkTypeface> FontDataManager::onLegacyMakeTypeface(
    const char requested_family_name[],
    SkFontStyle requested_style) const {
  return onMatchFamilyStyle(requested_family_name, requested_style);
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

}  // namespace font_data_service
