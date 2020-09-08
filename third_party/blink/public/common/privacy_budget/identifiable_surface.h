// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABLE_SURFACE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABLE_SURFACE_H_

#include <stdint.h>

#include <cstddef>
#include <functional>
#include <tuple>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"

namespace blink {

// An identifiable surface.
//
// This class intends to be a lightweight wrapper over a simple integer. It
// exhibits the following characteristics:
//
//   * All methods are constexpr.
//   * Immutable.
//   * Efficient enough to pass by value.
//
// Internally, an identifiable surface is represented as a 64-bit unsigned
// integer that can be used as the metric hash for reporting metrics via UKM.
//
// The least-significant |kTypeBits| of the value is used to store
// a IdentifiableSurface::Type value. The remainder stores the 56
// least-significant bits of an IdentifiableToken.
class IdentifiableSurface {
 public:
  // Number of bits used by Type.
  static constexpr int kTypeBits = 8;

  // Bitmask for extracting Type value from a surface hash.
  static constexpr uint64_t kTypeMask = (1 << kTypeBits) - 1;

  // Indicator for an uninitialized IdentifiableSurface. Maps to
  // {Type::kReservedInternal, 0} which is not possible for a valid surface.
  static constexpr uint64_t kInvalidHash = 0;

  // HTML canvas readback -- bits [0-3] of the 64-bit input are the context type
  // (Type::kCanvasReadback), bits [4-6] are skipped ops, sensitive ops, and
  // partial image ops bits, respectively. The remaining bits are for the canvas
  // operations digest. If the digest wasn't calculated (there's no digest for
  // webgl, for instance), the digest field is 0.
  enum CanvasTaintBit : uint64_t {
    // At least one drawing operation didn't update the digest -- this is ether
    // due to performance or resource consumption reasons.
    kSkipped = UINT64_C(0x10),

    // At least one drawing operation operated on a sensitive string. Sensitive
    // strings use a 16-bit hash digest.
    kSensitive = UINT64_C(0x20),

    // At least one drawing operation was only partially digested, for
    // performance reasons.
    kPartiallyDigested = UINT64_C(0x40)
  };

  // Type of identifiable surface.
  //
  // Even though the data type is uint64_t, we can only use 8 bits due to how we
  // pack the surface type and a digest of the input into a 64 bits.
  //
  // These values are used for aggregation across versions. Entries should not
  // be renumbered and numeric values should never be reused.
  enum class Type : uint64_t {
    // This type is reserved for internal use and should not be used for
    // reporting any identifiability metrics.
    kReservedInternal = 0,

    // Input is a mojom::WebFeature
    kWebFeature = 1,

    // Represents a readback of a canvas. Input is the
    // CanvasRenderingContextType.
    kCanvasReadback = 2,

    // Represents loading a font locally based on a name lookup that is allowed
    // to match either a unique name or a family name. This occurs when a
    // font-family CSS rule doesn't match any @font-face rule. Input is the
    // combination of the lookup name and the FontSelectionRequest (i.e. weight,
    // width and slope).
    kLocalFontLookupByUniqueOrFamilyName = 3,

    // Represents looking up the family name of a generic font. Input is the
    // combination of the generic font family name, script code and
    // GenericFamilyType.
    kGenericFontLookup = 4,

    // Represents an attempt to access files made publicly accessible by
    // extensions via web_accessible_resources. This may be recorded both in the
    // renderer and the browser. Browser-side events will be associated with
    // the top frame's navigation ID, not a child frame. Render-side events are
    // associated with document's ID.
    kExtensionFileAccess = 5,

    // Extension running content-script.
    kExtensionContentScript = 6,

    // Represents making a measurement of one of the above surfacess. This
    // metric is retained even if filtering discards the surface.
    kMeasuredSurface = 7,

    // WebGL parameter for WebGLRenderingContext.getParameter().
    kWebGLParameter = 8,

    // Represents a call to |MediaRecorder.isTypeSupported(mimeType)|. Input is
    // the mime type supplied to the method.
    kMediaRecorder_IsTypeSupported = 9,

    // Represents a call to |MediaSource.isTypeSupported(mimeType)|. Input is
    // the mime type supplied to the method.
    kMediaSource_IsTypeSupported = 10,

    // Represents a call to |HTMLMediaElement.canPlayType(mimeType)|. Input is
    // the mime type supplied to the method.
    kHTMLMediaElement_CanPlayType = 11,

    // Represents loading a font locally based on a name lookup that is only
    // allowed to match a unique name. This occurs in @font-face CSS rules with
    // a src:local attribute. Input is the combination of the lookup name and
    // the FontSelectionRequest (i.e. weight, width and slope).
    kLocalFontLookupByUniqueNameOnly = 12,

    // Represents loading a font locally based on a fallback character. Input is
    // the combination of the fallback character, FallbackPriority and the
    // FontSelectionRequest (i.e. weight, width and slope).
    kLocalFontLookupByFallbackCharacter = 13,

    // Represents loading a font locally as a last resort. Input is the
    // FontSelectionRequest (i.e. weight, width and slope).
    kLocalFontLookupAsLastResort = 14,

    // We can use values up to and including |kMax|.
    kMax = (1 << kTypeBits) - 1
  };

  // Default constructor is invalid.
  IdentifiableSurface() : IdentifiableSurface(kInvalidHash) {}

  // Construct an IdentifiableSurface based on a precalculated metric hash. Can
  // also be used as the first step in decoding an encoded metric hash.
  static constexpr IdentifiableSurface FromMetricHash(uint64_t metric_hash) {
    return IdentifiableSurface(metric_hash);
  }

  // Construct an IdentifiableSurface based on a surface type and an input hash.
  static constexpr IdentifiableSurface FromTypeAndInput(Type type,
                                                        uint64_t input) {
    return IdentifiableSurface(KeyFromSurfaceTypeAndInput(type, input));
  }

  // Construct an IdentifiableSurface based on a surface type and an input
  // token.
  static constexpr IdentifiableSurface FromTypeAndToken(
      Type type,
      IdentifiableToken token) {
    return IdentifiableSurface(KeyFromSurfaceTypeAndInput(type, token.value_));
  }

  // Construct an invalid identifiable surface.
  static constexpr IdentifiableSurface Invalid() {
    return IdentifiableSurface(kInvalidHash);
  }

  // Returns the UKM metric hash corresponding to this IdentifiableSurface.
  constexpr uint64_t ToUkmMetricHash() const { return metric_hash_; }

  // Returns the type of this IdentifiableSurface.
  constexpr Type GetType() const {
    return std::get<0>(SurfaceTypeAndInputFromMetricKey(metric_hash_));
  }

  // Returns the input hash for this IdentifiableSurface.
  //
  // The value that's returned can be different from what's used for
  // constructing the IdentifiableSurface via FromTypeAndInput() if the input is
  // >= 2^56.
  constexpr uint64_t GetInputHash() const {
    return std::get<1>(SurfaceTypeAndInputFromMetricKey(metric_hash_));
  }

  constexpr bool IsValid() const { return metric_hash_ != kInvalidHash; }

 private:
  constexpr explicit IdentifiableSurface(uint64_t metric_hash)
      : metric_hash_(metric_hash) {}

  // Returns a 64-bit metric key given an IdentifiableSurfaceType and a 64 bit
  // input digest.
  //
  // The returned key can be used as the metric hash when invoking
  // UkmEntryBuilderBase::SetMetricInternal().
  static constexpr uint64_t KeyFromSurfaceTypeAndInput(Type type,
                                                       uint64_t input) {
    uint64_t type_as_int = static_cast<uint64_t>(type);
    return type_as_int | (input << kTypeBits);
  }

  // Returns the IdentifiableSurfaceType and the input hash given a metric key.
  //
  // This is approximately the inverse of MetricKeyFromSurfaceTypeAndInput().
  // See caveat in GetInputHash() about cases where the input hash can differ
  // from that used to construct this IdentifiableSurface.
  static constexpr std::tuple<Type, uint64_t> SurfaceTypeAndInputFromMetricKey(
      uint64_t metric) {
    return std::make_tuple(static_cast<Type>(metric & kTypeMask),
                           metric >> kTypeBits);
  }

  uint64_t metric_hash_;
};

constexpr bool operator<(const IdentifiableSurface& left,
                         const IdentifiableSurface& right) {
  return left.ToUkmMetricHash() < right.ToUkmMetricHash();
}

constexpr bool operator<=(const IdentifiableSurface& left,
                          const IdentifiableSurface& right) {
  return left.ToUkmMetricHash() <= right.ToUkmMetricHash();
}

constexpr bool operator>(const IdentifiableSurface& left,
                         const IdentifiableSurface& right) {
  return left.ToUkmMetricHash() > right.ToUkmMetricHash();
}

constexpr bool operator>=(const IdentifiableSurface& left,
                          const IdentifiableSurface& right) {
  return left.ToUkmMetricHash() >= right.ToUkmMetricHash();
}

constexpr bool operator==(const IdentifiableSurface& left,
                          const IdentifiableSurface& right) {
  return left.ToUkmMetricHash() == right.ToUkmMetricHash();
}

constexpr bool operator!=(const IdentifiableSurface& left,
                          const IdentifiableSurface& right) {
  return left.ToUkmMetricHash() != right.ToUkmMetricHash();
}

// Hash function compatible with std::hash.
struct IdentifiableSurfaceHash {
  size_t operator()(const IdentifiableSurface& s) const {
    return std::hash<uint64_t>{}(s.ToUkmMetricHash());
  }
};

// Compare function compatible with std::less
struct IdentifiableSurfaceCompLess {
  bool operator()(const IdentifiableSurface& lhs,
                  const IdentifiableSurface& rhs) const {
    return lhs.ToUkmMetricHash() < rhs.ToUkmMetricHash();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABLE_SURFACE_H_
