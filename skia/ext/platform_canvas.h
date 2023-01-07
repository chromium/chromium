// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_PLATFORM_CANVAS_H_
#define SKIA_EXT_PLATFORM_CANVAS_H_

#include <stddef.h>
#include <stdint.h>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

// The platform-specific device will include the necessary platform headers
// to get the surface type.

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"

// A PlatformCanvas is a software-rasterized SkCanvas which is *also*
// addressable by the platform-specific drawing API (GDI, Core Graphics,
// Cairo...).

namespace skia {

//
//  Note about error handling.
//
//  Creating a canvas can fail at times, most often because we fail to allocate
//  the backing-store (pixels). This can be from out-of-memory, or something
//  more opaque, like GDI or cairo reported a failure.
//
//  To allow the caller to handle the failure, every Create... factory takes an
//  enum as its last parameter. The default value is kCrashOnFailure. If the
//  caller passes kReturnNullOnFailure, then the caller is responsible to check
//  the return result.
//
enum OnFailureType {
  CRASH_ON_FAILURE,
  RETURN_NULL_ON_FAILURE
};

#if defined(WIN32)
  // The shared_section parameter is passed to gfx::PlatformDevice::create.
  // See it for details.
SK_API std::unique_ptr<SkCanvas> CreatePlatformCanvasWithSharedSection(
    int width,
    int height,
    bool is_opaque,
    HANDLE shared_section,
    OnFailureType failure_type);

// Returns the NativeDrawingContext to use for native platform drawing calls.
SK_API HDC GetNativeDrawingContext(SkCanvas* canvas);

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__sun) || defined(ANDROID) || defined(__APPLE__) ||             \
    defined(__Fuchsia__)
// Construct a canvas from the given memory region. The memory is not cleared
// first. @data must be, at least, @height * StrideForWidth(@width) bytes.
SK_API std::unique_ptr<SkCanvas> CreatePlatformCanvasWithPixels(
    int width,
    int height,
    bool is_opaque,
    uint8_t* data,
    OnFailureType failure_type);
#endif

inline std::unique_ptr<SkCanvas> CreatePlatformCanvas(int width,
                                                      int height,
                                                      bool is_opaque) {
#if defined(WIN32)
  return CreatePlatformCanvasWithSharedSection(width, height, is_opaque, 0,
                                               CRASH_ON_FAILURE);
#else
  return CreatePlatformCanvasWithPixels(width, height, is_opaque, nullptr,
                                        CRASH_ON_FAILURE);
#endif
}

inline std::unique_ptr<SkCanvas> TryCreateBitmapCanvas(int width,
                                                       int height,
                                                       bool is_opaque) {
#if defined(WIN32)
  return CreatePlatformCanvasWithSharedSection(width, height, is_opaque, 0,
                                               RETURN_NULL_ON_FAILURE);
#else
  return CreatePlatformCanvasWithPixels(width, height, is_opaque, nullptr,
                                        RETURN_NULL_ON_FAILURE);
#endif
}

// Copies pixels from the SkCanvas into an SkBitmap, fetching pixels from
// GPU memory if necessary.
//
// The bitmap will remain empty if we can't allocate enough memory for a copy
// of the pixels.
SK_API SkBitmap ReadPixels(SkCanvas* canvas);

// Gives the pixmap passed in *writable* access to the pixels backing this
// canvas. All writes to the pixmap should be visible if the canvas is
// raster-backed.
//
// Returns false on failure: if either argument is nullptr, or if the
// pixels can not be retrieved from the canvas. In the latter case resets
// the pixmap to empty.
SK_API bool GetWritablePixels(SkCanvas* canvas, SkPixmap* pixmap);

}  // namespace skia

#endif  // SKIA_EXT_PLATFORM_CANVAS_H_
