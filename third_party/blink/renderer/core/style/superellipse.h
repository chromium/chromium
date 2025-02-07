// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SUPERELLIPSE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SUPERELLIPSE_H_

#include <limits>
namespace blink {

// Represents a superellipse, as defined in
// https://drafts.csswg.org/css-borders-4/#funcdef-superellipse
class Superellipse {
 public:
  // https://drafts.csswg.org/css-borders-4/#valdef-corner-shape-value-bevel
  static Superellipse Bevel() { return Superellipse(1.); }

  // https://drafts.csswg.org/css-borders-4/#valdef-corner-shape-value-notch
  static Superellipse Notch() { return Superellipse(0.); }

  // https://drafts.csswg.org/css-borders-4/#valdef-corner-shape-value-round
  static Superellipse Round() { return Superellipse(2); }

  // https://drafts.csswg.org/css-borders-4/#valdef-corner-shape-value-scoop
  static Superellipse Scoop() { return Superellipse(0.5); }

  // https://drafts.csswg.org/css-borders-4/#valdef-corner-shape-value-squircle
  static Superellipse Squircle() { return Superellipse(4); }

  // https://drafts.csswg.org/css-borders-4/#valdef-corner-shape-value-straight
  static Superellipse Straight() {
    return Superellipse(std::numeric_limits<double>::max());
  }

  explicit Superellipse(double exponent) : exponent_(exponent) {}

  // https://drafts.csswg.org/css-borders-4/#superellipse-exponent
  double Exponent() const { return exponent_; }

  bool operator==(const Superellipse& other) const = default;

 private:
  double exponent_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SUPERELLIPSE_H_
