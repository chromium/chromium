
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SKIA_CONFIG_SKUSERCONFIG_H_
#define SKIA_CONFIG_SKUSERCONFIG_H_

/*  SkTypes.h, the root of the public header files, includes this file
    SkUserConfig.h after first initializing certain Skia defines, letting
    this file change or augment those flags.

    Below are optional defines that add, subtract, or change default behavior
    in Skia. Your port can locally edit this file to enable/disable flags as
    you choose, or these can be declared on your command line (i.e. -Dfoo).

    By default, this #include file will always default to having all the flags
    commented out, so including it will have no effect.
*/

#include "base/component_export.h"
#include "skia/ext/skia_histogram.h"

// ===== Begin Chrome-specific definitions =====

#ifdef DCHECK_ALWAYS_ON
    #undef SK_RELEASE
    #define SK_DEBUG
#endif

/*  Define this to provide font subsetter for font subsetting when generating
    PDF documents.
 */
#define SK_PDF_USE_HARFBUZZ_SUBSET

/*  This controls how much space should be pre-allocated in an SkCanvas object
    to store the SkMatrix and clip via calls to SkCanvas::save() (and balanced
    with SkCanvas::restore()).
*/
#define SK_CANVAS_SAVE_RESTORE_PREALLOC_COUNT 16

// Handle exporting using base/component_export.h
#define SK_API COMPONENT_EXPORT(SKIA)

// Chromium does not use these fonts.  This define causes type1 fonts to be
// converted to type3 when producing PDFs, and reduces build size.
#define SK_PDF_DO_NOT_SUPPORT_TYPE_1_FONTS

#ifdef SK_DEBUG
#define SK_REF_CNT_MIXIN_INCLUDE "skia/config/sk_ref_cnt_ext_debug.h"
#else
#define SK_REF_CNT_MIXIN_INCLUDE "skia/config/sk_ref_cnt_ext_release.h"
#endif

// Log the file and line number for assertions.
#define SkDebugf(...) SkDebugf_FileLine(__FILE__, __LINE__, __VA_ARGS__)
SK_API void SkDebugf_FileLine(const char* file,
                              int line,
                              const char* format,
                              ...);

#define SK_ABORT(format, ...) SkAbort_FileLine(__FILE__, __LINE__, \
                                               format,##__VA_ARGS__)
[[noreturn]] SK_API void SkAbort_FileLine(const char* file,
                                          int line,
                                          const char* format,
                                          ...);

#if !defined(ANDROID)   // On Android, we use the skia default settings.
#define SK_A32_SHIFT    24
#define SK_R32_SHIFT    16
#define SK_G32_SHIFT    8
#define SK_B32_SHIFT    0
#endif

#if defined(SK_BUILD_FOR_MAC)

#define SK_CPU_LENDIAN
#undef  SK_CPU_BENDIAN

#elif defined(SK_BUILD_FOR_UNIX) || defined(SK_BUILD_FOR_ANDROID)

// Prefer FreeType's emboldening algorithm to Skia's
// TODO: skia used to just use hairline, but has improved since then, so
// we should revisit this choice...
#define SK_USE_FREETYPE_EMBOLDEN

#if defined(SK_BUILD_FOR_UNIX) && defined(SK_CPU_BENDIAN)
// Above we set the order for ARGB channels in registers. I suspect that, on
// big endian machines, you can keep this the same and everything will work.
// The in-memory order will be different, of course, but as long as everything
// is reading memory as words rather than bytes, it will all work. However, if
// you find that colours are messed up I thought that I would leave a helpful
// locator for you. Also see the comments in
// base/gfx/bitmap_platform_device_linux.h
#error Read the comment at this location
#endif

#endif

#if defined(__has_attribute)
#if __has_attribute(trivial_abi)
#define SK_TRIVIAL_ABI [[clang::trivial_abi]]
#else
#define SK_TRIVIAL_ABI
#endif
#else
#define SK_TRIVIAL_ABI
#endif

// These flags are no longer defined in Skia, but we have them (temporarily)
// until we update our call-sites (typically these are for API changes).
//
// Remove these as we update our sites.

// Workaround for poor anisotropic mipmap quality,
// pending Skia ripmap support.
// (https://bugs.chromium.org/p/skia/issues/detail?id=4863)
#define SK_SUPPORT_LEGACY_ANISOTROPIC_MIPMAP_SCALE

// Max. verb count for paths rendered by the edge-AA tessellating path renderer.
#define GR_AA_TESSELLATOR_MAX_VERB_COUNT 100

#define SK_USE_LEGACY_MIPMAP_BUILDER

#define SK_SUPPORT_LEGACY_CONIC_CHOP

#define SK_USE_PADDED_BLUR_UPSCALE

#define SK_LEGACY_INITWITHPREV_LAYER_SIZING

#define SK_AVOID_SLOW_RASTER_PIPELINE_BLURS

#define SK_DISABLE_LEGACY_NONRECORDER_IMAGE_APIS

#define SK_SUPPORT_LEGACY_RRECT_TRANSFORM

#define SK_ENABLE_SKOTTIE_FILLRULE

// Ensures Chromium is not using any mutable path APIs.  Only remove after the
// editing methods on SkPath are truly gone.
#define SK_HIDE_PATH_EDIT_METHODS

///////////////////////// Imported from BUILD.gn and skia_common.gypi

/* In some places Skia can use static initializers for global initialization,
 *  or fall back to lazy runtime initialization. Chrome always wants the latter.
 */
#define SK_ALLOW_STATIC_GLOBAL_INITIALIZERS 0

/* Restrict formats for Skia font matching to SFNT type fonts. */
#define SK_FONT_CONFIG_INTERFACE_ONLY_ALLOW_SFNT_FONTS

// Temporarily enable new strike cache pinning logic, for staging.
#define SK_STRIKE_CACHE_DOESNT_AUTO_CHECK_PINNERS

#define SK_USE_DISCARDABLE_SCALEDIMAGECACHE

#define SK_ATTR_DEPRECATED          SK_NOTHING_ARG1

// glGetError() forces a sync with gpu process on chrome
#define GR_GL_CHECK_ERROR_START 0

#endif  // SKIA_CONFIG_SKUSERCONFIG_H_
