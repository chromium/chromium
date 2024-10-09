
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SKIA_CONFIG_SKUSERCONFIG_H_
#define SKIA_CONFIG_SKUSERCONFIG_H_

/*  SkTypes.h, the root of the public header files, does the following trick:

    #include "include/config/SkUserConfig.h"
    #include "include/core/SkPostConfig.h"
    #include "include/core/SkPreConfig.h"

    SkPreConfig.h runs first, and it is responsible for initializing certain
    skia defines.

    SkPostConfig.h runs last, and its job is to just check that the final
    defines are consistent (i.e. that we don't have mutually conflicting
    defines).

    SkUserConfig.h (this file) runs in the middle. It gets to change or augment
    the list of flags initially set in preconfig, and then postconfig checks
    that everything still makes sense.

    Below are optional defines that add, subtract, or change default behavior
    in Skia. Your port can locally edit this file to enable/disable flags as
    you choose, or these can be delared on your command line (i.e. -Dfoo).

    By default, this include file will always default to having all of the flags
    commented out, so including it will have no effect.
*/

///////////////////////////////////////////////////////////////////////////////

/*  Skia has lots of debug-only code. Often this is just null checks or other
    parameter checking, but sometimes it can be quite intrusive (e.g. check that
    each 32bit pixel is in premultiplied form). This code can be very useful
    during development, but will slow things down in a shipping product.

    By default, these mutually exclusive flags are defined in SkPreConfig.h,
    based on the presence or absence of NDEBUG, but that decision can be changed
    here.
 */
//#define SK_DEBUG
//#define SK_RELEASE

/*  Skia has certain debug-only code that is extremely intensive even for debug
    builds.  This code is useful for diagnosing specific issues, but is not
    generally applicable, therefore it must be explicitly enabled to avoid
    the performance impact. By default these flags are undefined, but can be
    enabled by uncommenting them below.
 */
//#define SK_DEBUG_GLYPH_CACHE
//#define SK_DEBUG_PATH

/*  preconfig will have attempted to determine the endianness of the system,
    but you can change these mutually exclusive flags here.
 */
//#define SK_CPU_BENDIAN
//#define SK_CPU_LENDIAN

/*  Most compilers use the same bit endianness for bit flags in a byte as the
    system byte endianness, and this is the default. If for some reason this
    needs to be overridden, specify which of the mutually exclusive flags to
    use. For example, some atom processors in certain configurations have big
    endian byte order but little endian bit orders.
*/
//#define SK_UINT8_BITFIELD_BENDIAN
//#define SK_UINT8_BITFIELD_LENDIAN


/*  To write debug messages to a console, skia will call SkDebugf(...) following
    printf conventions (e.g. const char* format, ...). If you want to redirect
    this to something other than printf, define yours here
 */
//#define SkDebugf(...)  MyFunction(__VA_ARGS__)

/*
 *  To specify a different default font cache limit, define this. If this is
 *  undefined, skia will use a built-in value.
 */
//#define SK_DEFAULT_FONT_CACHE_LIMIT   (1024 * 1024)

/*
 *  To specify the default size of the image cache, undefine this and set it to
 *  the desired value (in bytes). SkGraphics.h as a runtime API to set this
 *  value as well. If this is undefined, a built-in value will be used.
 */
//#define SK_DEFAULT_IMAGE_CACHE_LIMIT (1024 * 1024)

/*  Define this to set the upper limit for text to support LCD. Values that
    are very large increase the cost in the font cache and draw slower, without
    improving readability. If this is undefined, Skia will use its default
    value (e.g. 48)
 */
//#define SK_MAX_SIZE_FOR_LCDTEXT     48

/*  Change the kN32_SkColorType ordering to BGRA to work in X windows.
 */
//#define SK_R32_SHIFT    16


/* Determines whether to build code that supports the Ganesh GPU backend. Some classes
   that are not GPU-specific, such as SkShader subclasses, have optional code
   that is used allows them to interact with this GPU backend. If you'd like to
   include this code, include -DSK_GANESH in your cflags or uncomment below.
   Defaults to not set (No Ganesh GPU backend).
   This define affects the ABI of Skia, so make sure it matches the client which uses
   the compiled version of Skia.
*/
// #define SK_GANESH

/* Skia makes use of histogram logging macros to trace the frequency of
 * events. By default, Skia provides no-op versions of these macros.
 * Skia consumers can provide their own definitions of these macros to
 * integrate with their histogram collection backend.
 */
//#define SK_HISTOGRAM_BOOLEAN(name, sample)
//#define SK_HISTOGRAM_EXACT_LINEAR(name, sample, value_max)
//#define SK_HISTOGRAM_MEMORY_KB(name, sample)
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

// Temporarily insulate Chrome pixel tests from Skia LOD bias change on GPU.
#define SK_USE_LEGACY_MIPMAP_LOD_BIAS

// Max. verb count for paths rendered by the edge-AA tessellating path renderer.
#define GR_AA_TESSELLATOR_MAX_VERB_COUNT 100

#define SK_USE_LEGACY_MIPMAP_BUILDER

#define SK_SUPPORT_LEGACY_CONIC_CHOP

#define SK_USE_PADDED_BLUR_UPSCALE

#define SK_LEGACY_INITWITHPREV_LAYER_SIZING

#define SK_USE_LEGACY_BLUR_RASTER

#define SK_USE_LEGACY_BLUR_GANESH

#define SK_USE_LEGACY_BLUR_GRAPHITE

#define SK_USE_SRCOVER_FOR_FILTERS


///////////////////////// Imported from BUILD.gn and skia_common.gypi

/* In some places Skia can use static initializers for global initialization,
 *  or fall back to lazy runtime initialization. Chrome always wants the latter.
 */
#define SK_ALLOW_STATIC_GLOBAL_INITIALIZERS 0

/* Restrict formats for Skia font matching to SFNT type fonts. */
#define SK_FONT_CONFIG_INTERFACE_ONLY_ALLOW_SFNT_FONTS

// Temporarily enable new strike cache pinning logic, for staging.
#define SK_STRIKE_CACHE_DOESNT_AUTO_CHECK_PINNERS

#define SK_IGNORE_BLURRED_RRECT_OPT
#define SK_USE_DISCARDABLE_SCALEDIMAGECACHE

#define SK_ATTR_DEPRECATED          SK_NOTHING_ARG1

// glGetError() forces a sync with gpu process on chrome
#define GR_GL_CHECK_ERROR_START 0

#endif  // SKIA_CONFIG_SKUSERCONFIG_H_
