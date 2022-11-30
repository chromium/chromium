/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From trusted/ppb_browser_font_trusted.idl,
 *   modified Thu Mar 28 10:14:27 2013.
 */

#ifndef PPAPI_C_TRUSTED_PPB_BROWSER_FONT_TRUSTED_H_
#define PPAPI_C_TRUSTED_PPB_BROWSER_FONT_TRUSTED_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

#define PPB_BROWSERFONT_TRUSTED_INTERFACE_1_0 "PPB_BrowserFont_Trusted;1.0"
#define PPB_BROWSERFONT_TRUSTED_INTERFACE PPB_BROWSERFONT_TRUSTED_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_BrowserFont_Trusted</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  /**
   * Uses the user's default web page font (normally either the default serif
   * or sans serif font).
   */
  PP_BROWSERFONT_TRUSTED_FAMILY_DEFAULT = 0,
  /**
   * These families will use the default web page font corresponding to the
   * given family.
   */
  PP_BROWSERFONT_TRUSTED_FAMILY_SERIF = 1,
  PP_BROWSERFONT_TRUSTED_FAMILY_SANSSERIF = 2,
  PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE = 3
} PP_BrowserFont_Trusted_Family;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_BrowserFont_Trusted_Family, 4);

/**
 * Specifies the font weight. Normally users will only use NORMAL or BOLD.
 */
typedef enum {
  PP_BROWSERFONT_TRUSTED_WEIGHT_100 = 0,
  PP_BROWSERFONT_TRUSTED_WEIGHT_200 = 1,
  PP_BROWSERFONT_TRUSTED_WEIGHT_300 = 2,
  PP_BROWSERFONT_TRUSTED_WEIGHT_400 = 3,
  PP_BROWSERFONT_TRUSTED_WEIGHT_500 = 4,
  PP_BROWSERFONT_TRUSTED_WEIGHT_600 = 5,
  PP_BROWSERFONT_TRUSTED_WEIGHT_700 = 6,
  PP_BROWSERFONT_TRUSTED_WEIGHT_800 = 7,
  PP_BROWSERFONT_TRUSTED_WEIGHT_900 = 8,
  PP_BROWSERFONT_TRUSTED_WEIGHT_NORMAL = PP_BROWSERFONT_TRUSTED_WEIGHT_400,
  PP_BROWSERFONT_TRUSTED_WEIGHT_BOLD = PP_BROWSERFONT_TRUSTED_WEIGHT_700
} PP_BrowserFont_Trusted_Weight;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_BrowserFont_Trusted_Weight, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_BrowserFont_Trusted_Description {
  /**
   * Font face name as a string. This can also be an undefined var, in which
   * case the generic family will be obeyed. If the face is not available on
   * the system, the browser will attempt to do font fallback or pick a default
   * font.
   */
  struct PP_Var face;
  /**
   * When Create()ing a font and the face is an undefined var, the family
   * specifies the generic font family type to use. If the face is specified,
   * this will be ignored.
   *
   * When Describe()ing a font, the family will be the value you passed in when
   * the font was created. In other words, if you specify a face name, the
   * family will not be updated to reflect whether the font name you requested
   * is serif or sans serif.
   */
  PP_BrowserFont_Trusted_Family family;
  /**
   * Size in pixels.
   *
   * You can specify 0 to get the default font size. The default font size
   * may vary depending on the requested font. The typical example is that
   * the user may have a different font size for the default monospace font to
   * give it a similar optical size to the proportionally spaced fonts.
   */
  uint32_t size;
  /**
   * Normally you will use either normal or bold.
   */
  PP_BrowserFont_Trusted_Weight weight;
  PP_Bool italic;
  PP_Bool small_caps;
  /**
   * Adjustment to apply to letter and word spacing, respectively. Initialize
   * to 0 to get normal spacing. Negative values bring letters/words closer
   * together, positive values separate them.
   */
  int32_t letter_spacing;
  int32_t word_spacing;
  /**
   * Ensure that this struct is 48-bytes wide by padding the end.  In some
   * compilers, PP_Var is 8-byte aligned, so those compilers align this struct
   * on 8-byte boundaries as well and pad it to 16 bytes even without this
   * padding attribute.  This padding makes its size consistent across
   * compilers.
   */
  int32_t padding;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_BrowserFont_Trusted_Description, 48);

struct PP_BrowserFont_Trusted_Metrics {
  int32_t height;
  int32_t ascent;
  int32_t descent;
  int32_t line_spacing;
  int32_t x_height;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_BrowserFont_Trusted_Metrics, 20);

struct PP_BrowserFont_Trusted_TextRun {
  /**
   * This var must either be a string or a null/undefined var (which will be
   * treated as a 0-length string).
   */
  struct PP_Var text;
  /**
   * Set to PP_TRUE if the text is right-to-left.
   */
  PP_Bool rtl;
  /**
   * Set to PP_TRUE to force the directionality of the text regardless of
   * content
   */
  PP_Bool override_direction;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_BrowserFont_Trusted_TextRun, 24);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * Provides an interface for native browser text rendering.
 *
 * This API is "trusted" not for security reasons, but because it can not be
 * implemented efficiently when running out-of-process in Browser Client. In
 * this case, WebKit is in another process and every text call would require a
 * synchronous IPC to the renderer. It is, however, available to native
 * (non-NaCl) out-of-process PPAPI plugins since WebKit is available in the
 * plugin process.
 */
struct PPB_BrowserFont_Trusted_1_0 {
  /**
   * Returns a list of all available font families on the system. You can use
   * this list to decide whether to Create() a font.
   *
   * The return value will be a single string with null characters delimiting
   * the end of each font name. For example: "Arial\0Courier\0Times\0".
   *
   * Returns an undefined var on failure (this typically means you passed an
   * invalid instance).
   */
  struct PP_Var (*GetFontFamilies)(PP_Instance instance);
  /**
   * Returns a font which best matches the given description. The return value
   * will have a non-zero ID on success, or zero on failure.
   */
  PP_Resource (*Create)(
      PP_Instance instance,
      const struct PP_BrowserFont_Trusted_Description* description);
  /**
   * Returns PP_TRUE if the given resource is a Font. Returns PP_FALSE if the
   * resource is invalid or some type other than a Font.
   */
  PP_Bool (*IsFont)(PP_Resource resource);
  /**
   * Loads the description and metrics of the font into the given structures.
   * The description will be different than the description the font was
   * created with since it will be filled with the real values from the font
   * that was actually selected.
   *
   * The PP_Var in the description should be of type Void on input. On output,
   * this will contain the string and will have a reference count of 1. The
   * plugin is responsible for calling Release on this var.
   *
   * Returns PP_TRUE on success, PP_FALSE if the font is invalid or if the Var
   * in the description isn't Null (to prevent leaks).
   */
  PP_Bool (*Describe)(PP_Resource font,
                      struct PP_BrowserFont_Trusted_Description* description,
                      struct PP_BrowserFont_Trusted_Metrics* metrics);
  /**
   * Draws the text to the image buffer.
   *
   * The given point represents the baseline of the left edge of the font,
   * regardless of whether it is left-to-right or right-to-left (in the case of
   * RTL text, this will actually represent the logical end of the text).
   *
   * The clip is optional and may be NULL. In this case, the text will be
   * clipped to the image.
   *
   * The image_data_is_opaque flag indicates whether subpixel antialiasing can
   * be performed, if it is supported. When the image below the text is
   * opaque, subpixel antialiasing is supported and you should set this to
   * PP_TRUE to pick up the user's default preferences. If your plugin is
   * partially transparent, then subpixel antialiasing is not possible and
   * grayscale antialiasing will be used instead (assuming the user has
   * antialiasing enabled at all).
   */
  PP_Bool (*DrawTextAt)(PP_Resource font,
                        PP_Resource image_data,
                        const struct PP_BrowserFont_Trusted_TextRun* text,
                        const struct PP_Point* position,
                        uint32_t color,
                        const struct PP_Rect* clip,
                        PP_Bool image_data_is_opaque);
  /**
   * Returns the width of the given string. If the font is invalid or the var
   * isn't a valid string, this will return -1.
   *
   * Note that this function handles complex scripts such as Arabic, combining
   * accents, etc. so that adding the width of substrings won't necessarily
   * produce the correct width of the entire string.
   *
   * Returns -1 on failure.
   */
  int32_t (*MeasureText)(PP_Resource font,
                         const struct PP_BrowserFont_Trusted_TextRun* text);
  /**
   * Returns the character at the given pixel X position from the beginning of
   * the string. This handles complex scripts such as Arabic, where characters
   * may be combined or replaced depending on the context. Returns (uint32)-1
   * on failure.
   *
   * TODO(brettw) this function may be broken. See the CharPosRTL test. It
   * seems to tell you "insertion point" rather than painting position. This
   * is useful but maybe not what we intended here.
   */
  uint32_t (*CharacterOffsetForPixel)(
      PP_Resource font,
      const struct PP_BrowserFont_Trusted_TextRun* text,
      int32_t pixel_position);
  /**
   * Returns the horizontal advance to the given character if the string was
   * placed at the given position. This handles complex scripts such as Arabic,
   * where characters may be combined or replaced depending on context. Returns
   * -1 on error.
   */
  int32_t (*PixelOffsetForCharacter)(
      PP_Resource font,
      const struct PP_BrowserFont_Trusted_TextRun* text,
      uint32_t char_offset);
};

typedef struct PPB_BrowserFont_Trusted_1_0 PPB_BrowserFont_Trusted;
/**
 * @}
 */

#endif  /* PPAPI_C_TRUSTED_PPB_BROWSER_FONT_TRUSTED_H_ */

