// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HB_SHIM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HB_SHIM_H_

// clang-format off
#include <hb.h>
#include <hb-aat.h>
#include <hb-ot.h>
#include <hr.h>
// clang-format on

#include <stdint.h>

#include <cstddef>
#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "build/buildflag.h"
#include "third_party/blink/renderer/platform/fonts/buildflags.h"

namespace blink::hb_shim {

// Value types, structs, and constants in <hb.h> and <hr.h> have identical
// binary representations. Verify layout and constant parity at compile time.
static_assert(sizeof(hb_glyph_info_t) == sizeof(hr_glyph_info_t));
static_assert(alignof(hb_glyph_info_t) == alignof(hr_glyph_info_t));
static_assert(offsetof(hb_glyph_info_t, codepoint) ==
              offsetof(hr_glyph_info_t, codepoint));
static_assert(offsetof(hb_glyph_info_t, mask) ==
              offsetof(hr_glyph_info_t, mask));
static_assert(offsetof(hb_glyph_info_t, cluster) ==
              offsetof(hr_glyph_info_t, cluster));

static_assert(sizeof(hb_glyph_position_t) == sizeof(hr_glyph_position_t));
static_assert(alignof(hb_glyph_position_t) == alignof(hr_glyph_position_t));
static_assert(offsetof(hb_glyph_position_t, x_advance) ==
              offsetof(hr_glyph_position_t, x_advance));
static_assert(offsetof(hb_glyph_position_t, y_advance) ==
              offsetof(hr_glyph_position_t, y_advance));
static_assert(offsetof(hb_glyph_position_t, x_offset) ==
              offsetof(hr_glyph_position_t, x_offset));
static_assert(offsetof(hb_glyph_position_t, y_offset) ==
              offsetof(hr_glyph_position_t, y_offset));

static_assert(sizeof(hb_feature_t) == sizeof(hr_feature_t));
static_assert(alignof(hb_feature_t) == alignof(hr_feature_t));
static_assert(offsetof(hb_feature_t, tag) == offsetof(hr_feature_t, tag));
static_assert(offsetof(hb_feature_t, value) == offsetof(hr_feature_t, value));
static_assert(offsetof(hb_feature_t, start) == offsetof(hr_feature_t, start));
static_assert(offsetof(hb_feature_t, end) == offsetof(hr_feature_t, end));

static_assert(sizeof(hb_variation_t) == sizeof(hr_variation_t));
static_assert(alignof(hb_variation_t) == alignof(hr_variation_t));
static_assert(offsetof(hb_variation_t, tag) == offsetof(hr_variation_t, tag));
static_assert(offsetof(hb_variation_t, value) ==
              offsetof(hr_variation_t, value));

static_assert(sizeof(hb_glyph_extents_t) == sizeof(hr_glyph_extents_t));
static_assert(alignof(hb_glyph_extents_t) == alignof(hr_glyph_extents_t));
static_assert(offsetof(hb_glyph_extents_t, x_bearing) ==
              offsetof(hr_glyph_extents_t, x_bearing));
static_assert(offsetof(hb_glyph_extents_t, y_bearing) ==
              offsetof(hr_glyph_extents_t, y_bearing));
static_assert(offsetof(hb_glyph_extents_t, width) ==
              offsetof(hr_glyph_extents_t, width));
static_assert(offsetof(hb_glyph_extents_t, height) ==
              offsetof(hr_glyph_extents_t, height));

static_assert(sizeof(hb_ot_math_glyph_variant_t) ==
              sizeof(hr_ot_math_glyph_variant_t));
static_assert(offsetof(hb_ot_math_glyph_variant_t, glyph) ==
              offsetof(hr_ot_math_glyph_variant_t, glyph));
static_assert(offsetof(hb_ot_math_glyph_variant_t, advance) ==
              offsetof(hr_ot_math_glyph_variant_t, advance));

static_assert(sizeof(hb_ot_math_glyph_part_t) ==
              sizeof(hr_ot_math_glyph_part_t));
static_assert(offsetof(hb_ot_math_glyph_part_t, glyph) ==
              offsetof(hr_ot_math_glyph_part_t, glyph));
static_assert(offsetof(hb_ot_math_glyph_part_t, start_connector_length) ==
              offsetof(hr_ot_math_glyph_part_t, start_connector_length));
static_assert(offsetof(hb_ot_math_glyph_part_t, end_connector_length) ==
              offsetof(hr_ot_math_glyph_part_t, end_connector_length));
static_assert(offsetof(hb_ot_math_glyph_part_t, full_advance) ==
              offsetof(hr_ot_math_glyph_part_t, full_advance));
static_assert(offsetof(hb_ot_math_glyph_part_t, flags) ==
              offsetof(hr_ot_math_glyph_part_t, flags));

static_assert(sizeof(hb_ot_var_axis_info_t) == sizeof(hr_ot_var_axis_info_t));
static_assert(offsetof(hb_ot_var_axis_info_t, tag) ==
              offsetof(hr_ot_var_axis_info_t, tag));
static_assert(offsetof(hb_ot_var_axis_info_t, name_id) ==
              offsetof(hr_ot_var_axis_info_t, name_id));
static_assert(offsetof(hb_ot_var_axis_info_t, min_value) ==
              offsetof(hr_ot_var_axis_info_t, min_value));
static_assert(offsetof(hb_ot_var_axis_info_t, default_value) ==
              offsetof(hr_ot_var_axis_info_t, default_value));
static_assert(offsetof(hb_ot_var_axis_info_t, max_value) ==
              offsetof(hr_ot_var_axis_info_t, max_value));

static_assert(sizeof(hb_aat_layout_feature_selector_info_t) ==
              sizeof(hr_aat_layout_feature_selector_info_t));
static_assert(offsetof(hb_aat_layout_feature_selector_info_t, enable) ==
              offsetof(hr_aat_layout_feature_selector_info_t, enable));

static_assert(static_cast<uint32_t>(HB_GLYPH_FLAG_UNSAFE_TO_BREAK) ==
              static_cast<uint32_t>(HR_GLYPH_FLAG_UNSAFE_TO_BREAK));
static_assert(static_cast<uint32_t>(HB_GLYPH_FLAG_DEFINED) ==
              static_cast<uint32_t>(HR_GLYPH_FLAG_DEFINED));
static_assert(static_cast<uint32_t>(HB_MEMORY_MODE_READONLY) ==
              static_cast<uint32_t>(HR_MEMORY_MODE_READONLY));
static_assert(static_cast<uint32_t>(HB_MEMORY_MODE_WRITABLE) ==
              static_cast<uint32_t>(HR_MEMORY_MODE_WRITABLE));
static_assert(static_cast<uint32_t>(HB_DIRECTION_LTR) ==
              static_cast<uint32_t>(HR_DIRECTION_LTR));
static_assert(static_cast<uint32_t>(HB_DIRECTION_RTL) ==
              static_cast<uint32_t>(HR_DIRECTION_RTL));
static_assert(static_cast<uint32_t>(HB_DIRECTION_TTB) ==
              static_cast<uint32_t>(HR_DIRECTION_TTB));
static_assert(static_cast<uint32_t>(HB_DIRECTION_BTT) ==
              static_cast<uint32_t>(HR_DIRECTION_BTT));

// Distinct incomplete tag types prevent accidentally passing shimmed handles
// directly to raw `hb_*` or `hr_*` C functions.
struct Blob;
struct Buffer;
struct Face;
struct Font;
struct Set;
struct LanguageImpl;
using Language = const LanguageImpl*;
inline constexpr Language kLanguageInvalid = nullptr;

ALWAYS_INLINE bool IsHarfRustEnabled() {
#if BUILDFLAG(ENABLE_HARFRUST)
  // TODO(crbug.com/427409139): Wire to
  // RuntimeEnabledFeatures::HarfRustShapingEnabled() once all Blink font and
  // shaping call sites are migrated to hb_shim.
  return false;
#else
  return false;
#endif
}

template <typename T>
struct HandleTraits;

#define BLINK_HB_SHIM_DEFINE_HANDLE_TRAITS(Tag, HbType, HrType, HbDestroy,     \
                                           HrDestroy)                          \
  template <>                                                                  \
  struct HandleTraits<Tag> {                                                   \
    using Hb = HbType;                                                         \
    using Hr = HrType;                                                         \
    ALWAYS_INLINE static Hb* ToHb(Tag* p) { return reinterpret_cast<Hb*>(p); } \
    ALWAYS_INLINE static const Hb* ToHb(const Tag* p) {                        \
      return reinterpret_cast<const Hb*>(p);                                   \
    }                                                                          \
    ALWAYS_INLINE static Hr* ToHr(Tag* p) { return reinterpret_cast<Hr*>(p); } \
    ALWAYS_INLINE static const Hr* ToHr(const Tag* p) {                        \
      return reinterpret_cast<const Hr*>(p);                                   \
    }                                                                          \
    ALWAYS_INLINE static Tag* FromHb(Hb* p) {                                  \
      return reinterpret_cast<Tag*>(p);                                        \
    }                                                                          \
    ALWAYS_INLINE static Tag* FromHr(Hr* p) {                                  \
      return reinterpret_cast<Tag*>(p);                                        \
    }                                                                          \
    ALWAYS_INLINE static void Destroy(Tag* p,                                  \
                                      [[maybe_unused]] bool is_harfrust) {     \
      if (!p) {                                                                \
        return;                                                                \
      }                                                                        \
      if constexpr (BUILDFLAG(ENABLE_HARFRUST)) {                              \
        if (is_harfrust) {                                                     \
          HrDestroy(ToHr(p));                                                  \
          return;                                                              \
        }                                                                      \
      }                                                                        \
      HbDestroy(ToHb(p));                                                      \
    }                                                                          \
  }

BLINK_HB_SHIM_DEFINE_HANDLE_TRAITS(Blob,
                                   hb_blob_t,
                                   hr_blob_t,
                                   hb_blob_destroy,
                                   hr_blob_destroy);
BLINK_HB_SHIM_DEFINE_HANDLE_TRAITS(Buffer,
                                   hb_buffer_t,
                                   hr_buffer_t,
                                   hb_buffer_destroy,
                                   hr_buffer_destroy);
BLINK_HB_SHIM_DEFINE_HANDLE_TRAITS(Face,
                                   hb_face_t,
                                   hr_face_t,
                                   hb_face_destroy,
                                   hr_face_destroy);
BLINK_HB_SHIM_DEFINE_HANDLE_TRAITS(Font,
                                   hb_font_t,
                                   hr_font_t,
                                   hb_font_destroy,
                                   hr_font_destroy);
BLINK_HB_SHIM_DEFINE_HANDLE_TRAITS(Set,
                                   hb_set_t,
                                   hr_set_t,
                                   hb_set_destroy,
                                   hr_set_destroy);

#undef BLINK_HB_SHIM_DEFINE_HANDLE_TRAITS

ALWAYS_INLINE hb_language_t ToHbLanguage(Language lang) {
  return reinterpret_cast<hb_language_t>(lang);
}
ALWAYS_INLINE hr_language_t ToHrLanguage(Language lang) {
  return reinterpret_cast<hr_language_t>(lang);
}
ALWAYS_INLINE Language FromHbLanguage(hb_language_t lang) {
  return reinterpret_cast<Language>(lang);
}
ALWAYS_INLINE Language FromHrLanguage(hr_language_t lang) {
  return reinterpret_cast<Language>(lang);
}

// Smart pointer for shimmed handles. Records which backend created `ptr_` so
// delayed destruction (e.g. Oilpan GC sweeping after
// ScopedHarfRustShapingForTest restores the flag) always invokes the matching
// `*_destroy` function.
//
// The class name does not follow the Google C++ style guide because it is a
// temporary stand-in for hb::unique_ptr.
template <typename T>
class unique_ptr {
 public:
  using element_type = T;

  struct Deleter {
    bool is_harfrust = false;
    void operator()(T* p) const { HandleTraits<T>::Destroy(p, is_harfrust); }
  };

  unique_ptr() = default;
  // NOLINTNEXTLINE(google-explicit-constructor)
  unique_ptr(std::nullptr_t) {}
  explicit unique_ptr(T* p, bool is_harfrust = IsHarfRustEnabled())
      : ptr_(p, Deleter{is_harfrust}) {}
  unique_ptr(const unique_ptr&) = delete;
  unique_ptr& operator=(const unique_ptr&) = delete;
  unique_ptr(unique_ptr&&) noexcept = default;
  unique_ptr& operator=(unique_ptr&&) noexcept = default;
  ~unique_ptr() = default;

  T* get() const { return ptr_.get(); }
  T* release() { return ptr_.release(); }
  void reset(T* p = nullptr, bool is_harfrust = IsHarfRustEnabled()) {
    ptr_ = std::unique_ptr<T, Deleter>(p, Deleter{is_harfrust});
  }
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator T*() const { return ptr_.get(); }
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_.get(); }
  explicit operator bool() const { return static_cast<bool>(ptr_); }

 private:
  std::unique_ptr<T, Deleter> ptr_;
};

// The following shim function names do not follow the naming conventions in
// Blink because they are wrappers around HarfBuzz/HarfRust C APIs, and will be
// replaced with HarfRust APIs.

// --- Blob ---

ALWAYS_INLINE Blob* blob_create(const char* data,
                                unsigned int length,
                                hb_memory_mode_t mode,
                                void* user_data,
                                hb_destroy_func_t destroy) {
  if (IsHarfRustEnabled()) {
    return HandleTraits<Blob>::FromHr(hr_blob_create(
        data, length, static_cast<hr_memory_mode_t>(mode), user_data, destroy));
  }
  return HandleTraits<Blob>::FromHb(
      hb_blob_create(data, length, mode, user_data, destroy));
}

ALWAYS_INLINE unsigned int blob_get_length(Blob* blob) {
  if (IsHarfRustEnabled()) {
    return hr_blob_get_length(HandleTraits<Blob>::ToHr(blob));
  }
  return hb_blob_get_length(HandleTraits<Blob>::ToHb(blob));
}

// --- Face ---

ALWAYS_INLINE unsigned int face_count(Blob* blob) {
  if (IsHarfRustEnabled()) {
    return hr_face_count(HandleTraits<Blob>::ToHr(blob));
  }
  return hb_face_count(HandleTraits<Blob>::ToHb(blob));
}

ALWAYS_INLINE Face* face_create(Blob* blob, unsigned int index) {
  if (IsHarfRustEnabled()) {
    return HandleTraits<Face>::FromHr(
        hr_face_create(HandleTraits<Blob>::ToHr(blob), index));
  }
  return HandleTraits<Face>::FromHb(
      hb_face_create(HandleTraits<Blob>::ToHb(blob), index));
}

ALWAYS_INLINE unsigned int face_get_upem(const Face* face) {
  if (IsHarfRustEnabled()) {
    return hr_face_get_upem(
        const_cast<hr_face_t*>(HandleTraits<Face>::ToHr(face)));
  }
  return hb_face_get_upem(HandleTraits<Face>::ToHb(face));
}

ALWAYS_INLINE Blob* face_reference_table(const Face* face, hb_tag_t tag) {
  if (IsHarfRustEnabled()) {
    return HandleTraits<Blob>::FromHr(hr_face_reference_table(
        const_cast<hr_face_t*>(HandleTraits<Face>::ToHr(face)), tag));
  }
  return HandleTraits<Blob>::FromHb(
      hb_face_reference_table(HandleTraits<Face>::ToHb(face), tag));
}

// --- Font ---

ALWAYS_INLINE Font* font_create(Face* face) {
  if (IsHarfRustEnabled()) {
    return HandleTraits<Font>::FromHr(
        hr_font_create(HandleTraits<Face>::ToHr(face)));
  }
  return HandleTraits<Font>::FromHb(
      hb_font_create(HandleTraits<Face>::ToHb(face)));
}

ALWAYS_INLINE Font* font_create_sub_font(Font* parent) {
  if (IsHarfRustEnabled()) {
    return HandleTraits<Font>::FromHr(
        hr_font_create_sub_font(HandleTraits<Font>::ToHr(parent)));
  }
  return HandleTraits<Font>::FromHb(
      hb_font_create_sub_font(HandleTraits<Font>::ToHb(parent)));
}

ALWAYS_INLINE Face* font_get_face(Font* font) {
  if (IsHarfRustEnabled()) {
    return HandleTraits<Face>::FromHr(
        hr_font_get_face(HandleTraits<Font>::ToHr(font)));
  }
  return HandleTraits<Face>::FromHb(
      hb_font_get_face(HandleTraits<Font>::ToHb(font)));
}

ALWAYS_INLINE hb_bool_t font_get_nominal_glyph(Font* font,
                                               hb_codepoint_t unicode,
                                               hb_codepoint_t* glyph) {
  if (IsHarfRustEnabled()) {
    return hr_font_get_nominal_glyph(HandleTraits<Font>::ToHr(font), unicode,
                                     glyph);
  }
  return hb_font_get_nominal_glyph(HandleTraits<Font>::ToHb(font), unicode,
                                   glyph);
}

ALWAYS_INLINE void font_set_ptem(Font* font, float ptem) {
  if (IsHarfRustEnabled()) {
    hr_font_set_ptem(HandleTraits<Font>::ToHr(font), ptem);
  } else {
    hb_font_set_ptem(HandleTraits<Font>::ToHb(font), ptem);
  }
}

ALWAYS_INLINE void font_set_scale(Font* font, int x_scale, int y_scale) {
  if (IsHarfRustEnabled()) {
    hr_font_set_scale(HandleTraits<Font>::ToHr(font), x_scale, y_scale);
  } else {
    hb_font_set_scale(HandleTraits<Font>::ToHb(font), x_scale, y_scale);
  }
}

ALWAYS_INLINE void font_set_variations(Font* font,
                                       const hb_variation_t* variations,
                                       unsigned int variations_length) {
  if (IsHarfRustEnabled()) {
    hr_font_set_variations(HandleTraits<Font>::ToHr(font),
                           reinterpret_cast<const hr_variation_t*>(variations),
                           variations_length);
  } else {
    hb_font_set_variations(HandleTraits<Font>::ToHb(font), variations,
                           variations_length);
  }
}

ALWAYS_INLINE void ot_font_set_funcs(Font* font) {
  if (IsHarfRustEnabled()) {
    hr_ot_font_set_funcs(HandleTraits<Font>::ToHr(font));
  } else {
    hb_ot_font_set_funcs(HandleTraits<Font>::ToHb(font));
  }
}

// --- Buffer ---

ALWAYS_INLINE Buffer* buffer_create() {
  if (IsHarfRustEnabled()) {
    return HandleTraits<Buffer>::FromHr(hr_buffer_create());
  }
  return HandleTraits<Buffer>::FromHb(hb_buffer_create());
}

ALWAYS_INLINE void buffer_reset(Buffer* buffer) {
  if (IsHarfRustEnabled()) {
    hr_buffer_reset(HandleTraits<Buffer>::ToHr(buffer));
  } else {
    hb_buffer_reset(HandleTraits<Buffer>::ToHb(buffer));
  }
}

ALWAYS_INLINE void buffer_add(Buffer* buffer,
                              hb_codepoint_t codepoint,
                              unsigned int cluster) {
  if (IsHarfRustEnabled()) {
    hr_buffer_add(HandleTraits<Buffer>::ToHr(buffer), codepoint, cluster);
  } else {
    hb_buffer_add(HandleTraits<Buffer>::ToHb(buffer), codepoint, cluster);
  }
}

ALWAYS_INLINE void buffer_add_latin1(Buffer* buffer,
                                     const uint8_t* text,
                                     int text_length,
                                     unsigned int item_offset,
                                     int item_length) {
  if (IsHarfRustEnabled()) {
    hr_buffer_add_latin1(HandleTraits<Buffer>::ToHr(buffer), text, text_length,
                         item_offset, item_length);
  } else {
    hb_buffer_add_latin1(HandleTraits<Buffer>::ToHb(buffer), text, text_length,
                         item_offset, item_length);
  }
}

ALWAYS_INLINE void buffer_add_utf16(Buffer* buffer,
                                    const uint16_t* text,
                                    int text_length,
                                    unsigned int item_offset,
                                    int item_length) {
  if (IsHarfRustEnabled()) {
    hr_buffer_add_utf16(HandleTraits<Buffer>::ToHr(buffer), text, text_length,
                        item_offset, item_length);
  } else {
    hb_buffer_add_utf16(HandleTraits<Buffer>::ToHb(buffer), text, text_length,
                        item_offset, item_length);
  }
}

ALWAYS_INLINE void buffer_set_direction(Buffer* buffer,
                                        hb_direction_t direction) {
  if (IsHarfRustEnabled()) {
    hr_buffer_set_direction(HandleTraits<Buffer>::ToHr(buffer),
                            static_cast<hr_direction_t>(direction));
  } else {
    hb_buffer_set_direction(HandleTraits<Buffer>::ToHb(buffer), direction);
  }
}

ALWAYS_INLINE hb_direction_t buffer_get_direction(const Buffer* buffer) {
  if (IsHarfRustEnabled()) {
    return static_cast<hb_direction_t>(hr_buffer_get_direction(
        const_cast<hr_buffer_t*>(HandleTraits<Buffer>::ToHr(buffer))));
  }
  return hb_buffer_get_direction(HandleTraits<Buffer>::ToHb(buffer));
}

ALWAYS_INLINE void buffer_set_script(Buffer* buffer, hb_script_t script) {
  if (IsHarfRustEnabled()) {
    hr_buffer_set_script(HandleTraits<Buffer>::ToHr(buffer),
                         static_cast<hr_script_t>(script));
  } else {
    hb_buffer_set_script(HandleTraits<Buffer>::ToHb(buffer), script);
  }
}

ALWAYS_INLINE void buffer_set_language(Buffer* buffer, Language language) {
  if (IsHarfRustEnabled()) {
    hr_buffer_set_language(HandleTraits<Buffer>::ToHr(buffer),
                           ToHrLanguage(language));
  } else {
    hb_buffer_set_language(HandleTraits<Buffer>::ToHb(buffer),
                           ToHbLanguage(language));
  }
}

ALWAYS_INLINE unsigned int buffer_get_length(const Buffer* buffer) {
  if (IsHarfRustEnabled()) {
    return hr_buffer_get_length(
        const_cast<hr_buffer_t*>(HandleTraits<Buffer>::ToHr(buffer)));
  }
  return hb_buffer_get_length(HandleTraits<Buffer>::ToHb(buffer));
}

ALWAYS_INLINE hb_glyph_info_t* buffer_get_glyph_infos(Buffer* buffer,
                                                      unsigned int* length) {
  if (IsHarfRustEnabled()) {
    return reinterpret_cast<hb_glyph_info_t*>(
        hr_buffer_get_glyph_infos(HandleTraits<Buffer>::ToHr(buffer), length));
  }
  return hb_buffer_get_glyph_infos(HandleTraits<Buffer>::ToHb(buffer), length);
}

ALWAYS_INLINE hb_glyph_position_t* buffer_get_glyph_positions(
    Buffer* buffer,
    unsigned int* length) {
  if (IsHarfRustEnabled()) {
    return reinterpret_cast<hb_glyph_position_t*>(hr_buffer_get_glyph_positions(
        HandleTraits<Buffer>::ToHr(buffer), length));
  }
  return hb_buffer_get_glyph_positions(HandleTraits<Buffer>::ToHb(buffer),
                                       length);
}

// --- Set ---

ALWAYS_INLINE Set* set_create() {
  if (IsHarfRustEnabled()) {
    return HandleTraits<Set>::FromHr(hr_set_create());
  }
  return HandleTraits<Set>::FromHb(hb_set_create());
}

ALWAYS_INLINE void set_clear(Set* set) {
  if (IsHarfRustEnabled()) {
    hr_set_clear(HandleTraits<Set>::ToHr(set));
  } else {
    hb_set_clear(HandleTraits<Set>::ToHb(set));
  }
}

ALWAYS_INLINE hb_bool_t set_has(const Set* set, hb_codepoint_t codepoint) {
  if (IsHarfRustEnabled()) {
    return hr_set_has(HandleTraits<Set>::ToHr(set), codepoint);
  }
  return hb_set_has(HandleTraits<Set>::ToHb(set), codepoint);
}

// --- Shaping & Common ---

ALWAYS_INLINE void shape(Font* font,
                         Buffer* buffer,
                         const hb_feature_t* features,
                         unsigned int num_features) {
  if (IsHarfRustEnabled()) {
    hr_shape(HandleTraits<Font>::ToHr(font), HandleTraits<Buffer>::ToHr(buffer),
             reinterpret_cast<const hr_feature_t*>(features), num_features);
  } else {
    hb_shape(HandleTraits<Font>::ToHb(font), HandleTraits<Buffer>::ToHb(buffer),
             features, num_features);
  }
}

ALWAYS_INLINE Language language_from_string(const char* str, int len) {
  if (IsHarfRustEnabled()) {
    return FromHrLanguage(hr_language_from_string(str, len));
  }
  return FromHbLanguage(hb_language_from_string(str, len));
}

ALWAYS_INLINE hb_script_t script_from_string(const char* str, int len) {
  if (IsHarfRustEnabled()) {
    return static_cast<hb_script_t>(hr_script_from_string(str, len));
  }
  return hb_script_from_string(str, len);
}

ALWAYS_INLINE hb_tag_t tag_from_string(const char* str, int len) {
  if (IsHarfRustEnabled()) {
    return hr_tag_from_string(str, len);
  }
  return hb_tag_from_string(str, len);
}

// --- OpenType Layout / Baseline / Color / Math / Var / Name ---

ALWAYS_INLINE Language ot_tag_to_language(hb_tag_t tag) {
  if (IsHarfRustEnabled()) {
    return FromHrLanguage(hr_ot_tag_to_language(tag));
  }
  return FromHbLanguage(hb_ot_tag_to_language(tag));
}

ALWAYS_INLINE void ot_tags_from_script_and_language(
    hb_script_t script,
    Language language,
    unsigned int* script_count,
    hb_tag_t* script_tags,
    unsigned int* language_count,
    hb_tag_t* language_tags) {
  if (IsHarfRustEnabled()) {
    hr_ot_tags_from_script_and_language(
        static_cast<hr_script_t>(script), ToHrLanguage(language), script_count,
        script_tags, language_count, language_tags);
  } else {
    hb_ot_tags_from_script_and_language(script, ToHbLanguage(language),
                                        script_count, script_tags,
                                        language_count, language_tags);
  }
}

ALWAYS_INLINE hb_bool_t
ot_layout_get_baseline(Font* font,
                       hb_ot_layout_baseline_tag_t baseline_tag,
                       hb_direction_t direction,
                       hb_tag_t script_tag,
                       hb_tag_t language_tag,
                       hb_position_t* coord) {
  if (IsHarfRustEnabled()) {
    return hr_ot_layout_get_baseline(
        HandleTraits<Font>::ToHr(font),
        static_cast<hr_ot_layout_baseline_tag_t>(baseline_tag),
        static_cast<hr_direction_t>(direction), script_tag, language_tag,
        coord);
  }
  return hb_ot_layout_get_baseline(HandleTraits<Font>::ToHb(font), baseline_tag,
                                   direction, script_tag, language_tag, coord);
}

ALWAYS_INLINE hb_bool_t ot_layout_has_positioning(Face* face) {
  if (IsHarfRustEnabled()) {
    return hr_ot_layout_has_positioning(HandleTraits<Face>::ToHr(face));
  }
  return hb_ot_layout_has_positioning(HandleTraits<Face>::ToHb(face));
}

ALWAYS_INLINE hb_bool_t ot_layout_has_substitution(Face* face) {
  if (IsHarfRustEnabled()) {
    return hr_ot_layout_has_substitution(HandleTraits<Face>::ToHr(face));
  }
  return hb_ot_layout_has_substitution(HandleTraits<Face>::ToHb(face));
}

ALWAYS_INLINE hb_bool_t
ot_layout_table_select_script(Face* face,
                              hb_tag_t table_tag,
                              unsigned int script_count,
                              const hb_tag_t* script_tags,
                              unsigned int* script_index,
                              hb_tag_t* chosen_script) {
  if (IsHarfRustEnabled()) {
    return hr_ot_layout_table_select_script(
        HandleTraits<Face>::ToHr(face), table_tag, script_count, script_tags,
        script_index, chosen_script);
  }
  return hb_ot_layout_table_select_script(HandleTraits<Face>::ToHb(face),
                                          table_tag, script_count, script_tags,
                                          script_index, chosen_script);
}

ALWAYS_INLINE hb_bool_t
ot_layout_language_find_feature(Face* face,
                                hb_tag_t table_tag,
                                unsigned int script_index,
                                unsigned int language_index,
                                hb_tag_t feature_tag,
                                unsigned int* feature_index) {
  if (IsHarfRustEnabled()) {
    return hr_ot_layout_language_find_feature(
        HandleTraits<Face>::ToHr(face), table_tag, script_index, language_index,
        feature_tag, feature_index);
  }
  return hb_ot_layout_language_find_feature(
      HandleTraits<Face>::ToHb(face), table_tag, script_index, language_index,
      feature_tag, feature_index);
}

ALWAYS_INLINE unsigned int ot_layout_table_get_feature_tags(
    Face* face,
    hb_tag_t table_tag,
    unsigned int start_offset,
    unsigned int* feature_count,
    hb_tag_t* feature_tags) {
  if (IsHarfRustEnabled()) {
    return hr_ot_layout_table_get_feature_tags(HandleTraits<Face>::ToHr(face),
                                               table_tag, start_offset,
                                               feature_count, feature_tags);
  }
  return hb_ot_layout_table_get_feature_tags(HandleTraits<Face>::ToHb(face),
                                             table_tag, start_offset,
                                             feature_count, feature_tags);
}

ALWAYS_INLINE unsigned int ot_layout_table_get_lookup_count(
    Face* face,
    hb_tag_t table_tag) {
  if (IsHarfRustEnabled()) {
    return hr_ot_layout_table_get_lookup_count(HandleTraits<Face>::ToHr(face),
                                               table_tag);
  }
  return hb_ot_layout_table_get_lookup_count(HandleTraits<Face>::ToHb(face),
                                             table_tag);
}

ALWAYS_INLINE void ot_layout_lookup_collect_glyphs(Face* face,
                                                   hb_tag_t table_tag,
                                                   unsigned int lookup_index,
                                                   Set* glyphs_before,
                                                   Set* glyphs_input,
                                                   Set* glyphs_after,
                                                   Set* glyphs_output) {
  if (IsHarfRustEnabled()) {
    hr_ot_layout_lookup_collect_glyphs(HandleTraits<Face>::ToHr(face),
                                       table_tag, lookup_index,
                                       HandleTraits<Set>::ToHr(glyphs_before),
                                       HandleTraits<Set>::ToHr(glyphs_input),
                                       HandleTraits<Set>::ToHr(glyphs_after),
                                       HandleTraits<Set>::ToHr(glyphs_output));
  } else {
    hb_ot_layout_lookup_collect_glyphs(HandleTraits<Face>::ToHb(face),
                                       table_tag, lookup_index,
                                       HandleTraits<Set>::ToHb(glyphs_before),
                                       HandleTraits<Set>::ToHb(glyphs_input),
                                       HandleTraits<Set>::ToHb(glyphs_after),
                                       HandleTraits<Set>::ToHb(glyphs_output));
  }
}

ALWAYS_INLINE hb_bool_t ot_color_has_palettes(Face* face) {
  if (IsHarfRustEnabled()) {
    return hr_ot_color_has_palettes(HandleTraits<Face>::ToHr(face));
  }
  return hb_ot_color_has_palettes(HandleTraits<Face>::ToHb(face));
}

ALWAYS_INLINE unsigned int ot_color_palette_get_count(Face* face) {
  if (IsHarfRustEnabled()) {
    return hr_ot_color_palette_get_count(HandleTraits<Face>::ToHr(face));
  }
  return hb_ot_color_palette_get_count(HandleTraits<Face>::ToHb(face));
}

ALWAYS_INLINE hb_ot_color_palette_flags_t
ot_color_palette_get_flags(Face* face, unsigned int palette_index) {
  if (IsHarfRustEnabled()) {
    return static_cast<hb_ot_color_palette_flags_t>(
        hr_ot_color_palette_get_flags(HandleTraits<Face>::ToHr(face),
                                      palette_index));
  }
  return hb_ot_color_palette_get_flags(HandleTraits<Face>::ToHb(face),
                                       palette_index);
}

ALWAYS_INLINE unsigned int ot_color_palette_get_colors(
    Face* face,
    unsigned int palette_index,
    unsigned int start_offset,
    unsigned int* color_count,
    hb_color_t* colors) {
  if (IsHarfRustEnabled()) {
    return hr_ot_color_palette_get_colors(HandleTraits<Face>::ToHr(face),
                                          palette_index, start_offset,
                                          color_count, colors);
  }
  return hb_ot_color_palette_get_colors(HandleTraits<Face>::ToHb(face),
                                        palette_index, start_offset,
                                        color_count, colors);
}

ALWAYS_INLINE hb_bool_t ot_math_has_data(Face* face) {
  if (IsHarfRustEnabled()) {
    return hr_ot_math_has_data(HandleTraits<Face>::ToHr(face));
  }
  return hb_ot_math_has_data(HandleTraits<Face>::ToHb(face));
}

ALWAYS_INLINE hb_position_t ot_math_get_constant(Font* font,
                                                 hb_ot_math_constant_t c) {
  if (IsHarfRustEnabled()) {
    return hr_ot_math_get_constant(HandleTraits<Font>::ToHr(font),
                                   static_cast<hr_ot_math_constant_t>(c));
  }
  return hb_ot_math_get_constant(HandleTraits<Font>::ToHb(font), c);
}

ALWAYS_INLINE hb_position_t
ot_math_get_glyph_italics_correction(Font* font, hb_codepoint_t glyph) {
  if (IsHarfRustEnabled()) {
    return hr_ot_math_get_glyph_italics_correction(
        HandleTraits<Font>::ToHr(font), glyph);
  }
  return hb_ot_math_get_glyph_italics_correction(HandleTraits<Font>::ToHb(font),
                                                 glyph);
}

ALWAYS_INLINE unsigned int ot_math_get_glyph_variants(
    Font* font,
    hb_codepoint_t glyph,
    hb_direction_t direction,
    unsigned int start_offset,
    unsigned int* variants_count,
    hb_ot_math_glyph_variant_t* variants) {
  if (IsHarfRustEnabled()) {
    return hr_ot_math_get_glyph_variants(
        HandleTraits<Font>::ToHr(font), glyph,
        static_cast<hr_direction_t>(direction), start_offset, variants_count,
        reinterpret_cast<hr_ot_math_glyph_variant_t*>(variants));
  }
  return hb_ot_math_get_glyph_variants(HandleTraits<Font>::ToHb(font), glyph,
                                       direction, start_offset, variants_count,
                                       variants);
}

ALWAYS_INLINE unsigned int ot_math_get_glyph_assembly(
    Font* font,
    hb_codepoint_t glyph,
    hb_direction_t direction,
    unsigned int start_offset,
    unsigned int* parts_count,
    hb_ot_math_glyph_part_t* parts,
    hb_position_t* italics_correction) {
  if (IsHarfRustEnabled()) {
    return hr_ot_math_get_glyph_assembly(
        HandleTraits<Font>::ToHr(font), glyph,
        static_cast<hr_direction_t>(direction), start_offset, parts_count,
        reinterpret_cast<hr_ot_math_glyph_part_t*>(parts), italics_correction);
  }
  return hb_ot_math_get_glyph_assembly(HandleTraits<Font>::ToHb(font), glyph,
                                       direction, start_offset, parts_count,
                                       parts, italics_correction);
}

ALWAYS_INLINE hb_position_t
ot_math_get_min_connector_overlap(Font* font, hb_direction_t direction) {
  if (IsHarfRustEnabled()) {
    return hr_ot_math_get_min_connector_overlap(
        HandleTraits<Font>::ToHr(font), static_cast<hr_direction_t>(direction));
  }
  return hb_ot_math_get_min_connector_overlap(HandleTraits<Font>::ToHb(font),
                                              direction);
}

ALWAYS_INLINE unsigned int ot_var_get_axis_count(Face* face) {
  if (IsHarfRustEnabled()) {
    return hr_ot_var_get_axis_count(HandleTraits<Face>::ToHr(face));
  }
  return hb_ot_var_get_axis_count(HandleTraits<Face>::ToHb(face));
}

ALWAYS_INLINE unsigned int ot_var_get_axis_infos(
    Face* face,
    unsigned int start_offset,
    unsigned int* axes_count,
    hb_ot_var_axis_info_t* axes_array) {
  if (IsHarfRustEnabled()) {
    return hr_ot_var_get_axis_infos(
        HandleTraits<Face>::ToHr(face), start_offset, axes_count,
        reinterpret_cast<hr_ot_var_axis_info_t*>(axes_array));
  }
  return hb_ot_var_get_axis_infos(HandleTraits<Face>::ToHb(face), start_offset,
                                  axes_count, axes_array);
}

ALWAYS_INLINE unsigned int ot_name_get_utf16(Face* face,
                                             hb_ot_name_id_t name_id,
                                             Language language,
                                             unsigned int* text_size,
                                             uint16_t* text) {
  if (IsHarfRustEnabled()) {
    return hr_ot_name_get_utf16(HandleTraits<Face>::ToHr(face), name_id,
                                ToHrLanguage(language), text_size, text);
  }
  return hb_ot_name_get_utf16(HandleTraits<Face>::ToHb(face), name_id,
                              ToHbLanguage(language), text_size, text);
}

// --- AAT Layout ---

ALWAYS_INLINE unsigned int aat_layout_get_feature_types(
    Face* face,
    unsigned int start_offset,
    unsigned int* feature_count,
    hb_aat_layout_feature_type_t* features) {
  if (IsHarfRustEnabled()) {
    return hr_aat_layout_get_feature_types(
        HandleTraits<Face>::ToHr(face), start_offset, feature_count,
        reinterpret_cast<hr_aat_layout_feature_type_t*>(features));
  }
  return hb_aat_layout_get_feature_types(HandleTraits<Face>::ToHb(face),
                                         start_offset, feature_count, features);
}

ALWAYS_INLINE unsigned int aat_layout_feature_type_get_selector_infos(
    Face* face,
    hb_aat_layout_feature_type_t feature_type,
    unsigned int start_offset,
    unsigned int* selector_count,
    hb_aat_layout_feature_selector_info_t* selectors,
    unsigned int* default_index) {
  if (IsHarfRustEnabled()) {
    return hr_aat_layout_feature_type_get_selector_infos(
        HandleTraits<Face>::ToHr(face),
        static_cast<hr_aat_layout_feature_type_t>(feature_type), start_offset,
        selector_count,
        reinterpret_cast<hr_aat_layout_feature_selector_info_t*>(selectors),
        default_index);
  }
  return hb_aat_layout_feature_type_get_selector_infos(
      HandleTraits<Face>::ToHb(face), feature_type, start_offset,
      selector_count, selectors, default_index);
}

}  // namespace blink::hb_shim

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HB_SHIM_H_
