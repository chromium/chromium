// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/font_loader.h"

#import <Cocoa/Cocoa.h>
#include <CoreText/CoreText.h>

#include <limits>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"

namespace content {
namespace {

std::unique_ptr<FontLoader::ResultInternal> LoadFontOnFileThread(
    const std::u16string& font_name,
    const float font_point_size) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  NSString* font_name_ns = base::SysUTF16ToNSString(font_name);
  NSFont* font_to_encode =
      [NSFont fontWithName:font_name_ns size:font_point_size];

  // Load appropriate NSFont.
  if (!font_to_encode) {
    DLOG(ERROR) << "Failed to load font " << font_name;
    return nullptr;
  }

  // NSFont -> File path.
  // Warning: Calling this function on a font activated from memory will result
  // in failure with a -50 - paramErr.  This may occur if
  // CreateCGFontFromBuffer() is called in the same process as this function
  // e.g. when writing a unit test that exercises these two functions together.
  // If said unit test were to load a system font and activate it from memory
  // it becomes impossible for the system to the find the original file ref
  // since the font now lives in memory as far as it's concerned.
  CTFontRef ct_font = base::mac::NSToCFCast(font_to_encode);
  base::scoped_nsobject<NSURL> font_url(
      base::mac::CFToNSCast(base::mac::CFCastStrict<CFURLRef>(
          CTFontCopyAttribute(ct_font, kCTFontURLAttribute))));
  if (![font_url isFileURL]) {
    DLOG(ERROR) << "Failed to find font file for " << font_name;
    return nullptr;
  }

  base::FilePath font_path = base::mac::NSStringToFilePath([font_url path]);

  // Load file into shared memory buffer.
  int64_t font_file_size_64 = -1;
  if (!base::GetFileSize(font_path, &font_file_size_64)) {
    DLOG(ERROR) << "Couldn't get font file size for " << font_path.value();
    return nullptr;
  }

  if (font_file_size_64 <= 0 ||
      font_file_size_64 >= std::numeric_limits<int32_t>::max()) {
    DLOG(ERROR) << "Bad size for font file " << font_path.value();
    return nullptr;
  }

  auto result = std::make_unique<FontLoader::ResultInternal>();

  // Note: despite the fact that CTFontDescriptorFromBuffer() takes a uint32_t
  // size, this intentionally works with int32_t since base::ReadFile()'s max
  // size arg is an int...
  int32_t font_file_size_32 = base::checked_cast<int32_t>(font_file_size_64);
  auto region_and_mapping =
      base::ReadOnlySharedMemoryRegion::Create(font_file_size_32);

  if (!region_and_mapping.IsValid()) {
    DLOG(ERROR) << "Failed to create shmem area for " << font_name;
    return nullptr;
  }
  result->font_data = std::move(region_and_mapping.region);

  int32_t amt_read =
      base::ReadFile(font_path, region_and_mapping.mapping.GetMemoryAs<char>(),
                     font_file_size_32);
  if (amt_read != font_file_size_32) {
    DLOG(ERROR) << "Failed to read font data for " << font_path.value();
    return nullptr;
  }

  // Font loading used to call ATSFontGetContainer() and used that as font id.
  // ATS is deprecated. CoreText offers up the ATSFontRef typeface ID via
  // CTFontGetPlatformFont.
  result->font_id = CTFontGetPlatformFont(ct_font, nil);
  if (result->font_id == 0) {
    DLOG(ERROR) << "Failed to get font id for " << font_path.value();
    return nullptr;
  }

  return result;
}

void ReplyOnUIThread(FontLoader::LoadedCallback callback,
                     std::unique_ptr<FontLoader::ResultInternal> result) {
  if (!result) {
    std::move(callback).Run(base::ReadOnlySharedMemoryRegion(), 0);
    return;
  }

  DCHECK_NE(0u, result->font_id);
  std::move(callback).Run(std::move(result->font_data), result->font_id);
}

}  // namespace

FontLoader::ResultInternal::ResultInternal() = default;

FontLoader::ResultInternal::~ResultInternal() = default;

// static
void FontLoader::LoadFont(const std::u16string& font_name,
                          const float font_point_size,
                          LoadedCallback callback) {
  // Tasks are triggered when font loading in the sandbox fails. Usually due to
  // a user installing a third-party font manager. See crbug.com/72727. Web page
  // rendering can't continue until a font is returned.
  constexpr base::TaskTraits kTraits = {
      base::MayBlock(), base::TaskPriority::USER_VISIBLE,
      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTraits,
      base::BindOnce(&LoadFontOnFileThread, font_name, font_point_size),
      base::BindOnce(&ReplyOnUIThread, std::move(callback)));
}

// static
bool FontLoader::CTFontDescriptorFromBuffer(
    base::ReadOnlySharedMemoryRegion font_data,
    base::ScopedCFTypeRef<CTFontDescriptorRef>* out_descriptor) {
  out_descriptor->reset();
  base::ReadOnlySharedMemoryMapping mapping = font_data.Map();
  if (!mapping.IsValid())
    return false;

  NSData* data = [NSData dataWithBytes:mapping.memory() length:mapping.size()];
  base::ScopedCFTypeRef<CTFontDescriptorRef> data_descriptor(
      CTFontManagerCreateFontDescriptorFromData(base::mac::NSToCFCast(data)));

  if (!data_descriptor)
    return false;

  *out_descriptor = std::move(data_descriptor);
  return true;
}

// static
std::unique_ptr<FontLoader::ResultInternal> FontLoader::LoadFontForTesting(
    const std::u16string& font_name,
    const float font_point_size) {
  return LoadFontOnFileThread(font_name, font_point_size);
}

}  // namespace content
