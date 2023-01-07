// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_SIZE_H_
#define PPAPI_CPP_SIZE_H_

#include "ppapi/c/pp_size.h"
#include "ppapi/cpp/logging.h"

/// @file
/// This file defines the API to create a size based on width
/// and height.

namespace pp {

/// A size of an object based on width and height.
class Size {
 public:

  /// The default constructor. Initializes the width and height to 0.
  Size() {
    size_.width = 0;
    size_.height = 0;
  }

  /// A constructor accepting a pointer to a <code>PP_Size</code> and
  /// converting the <code>PP_Size</code> to a <code>Size</code>. This is an
  /// implicit conversion constructor.
  ///
  /// @param[in] s A pointer to a <code>PP_Size</code>.
  Size(const PP_Size& s) {  // Implicit.
    // Want the >= 0 checking of the setter.
    set_width(s.width);
    set_height(s.height);
  }

  /// A constructor accepting two int values for width and height and
  /// converting them to a <code>Size</code>.
  ///
  /// @param[in] w An int value representing a width.
  /// @param[in] h An int value representing a height.
  Size(int w, int h) {
    // Want the >= 0 checking of the setter.
    set_width(w);
    set_height(h);
  }

  /// Destructor.
  ~Size() {
  }

  /// PP_Size() allows implicit conversion of a <code>Size</code> to a
  /// <code>PP_Size</code>.
  ///
  /// @return A Size.
  operator PP_Size() {
    return size_;
  }

  /// Getter function for returning the internal <code>PP_Size</code> struct.
  ///
  /// @return A const reference to the internal <code>PP_Size</code> struct.
  const PP_Size& pp_size() const {
    return size_;
  }

  /// Getter function for returning the internal <code>PP_Size</code> struct.
  ///
  /// @return A mutable reference to the <code>PP_Size</code> struct.
  PP_Size& pp_size() {
    return size_;
  }

  /// Getter function for returning the value of width.
  ///
  /// @return The value of width for this <code>Size</code>.
  int width() const {
    return size_.width;
  }

  /// Setter function for setting the value of width.
  ///
  /// @param[in] w A new width value.
  void set_width(int w) {
    if (w < 0) {
      PP_DCHECK(w >= 0);
      w = 0;
    }
    size_.width = w;
  }

  /// Getter function for returning the value of height.
  ///
  /// @return The value of height for this <code>Size</code>.
  int height() const {
    return size_.height;
  }

  /// Setter function for setting the value of height.
  ///
  /// @param[in] h A new height value.
  void set_height(int h) {
    if (h < 0) {
      PP_DCHECK(h >= 0);
      h = 0;
    }
    size_.height = h;
  }

  /// GetArea() determines the area (width * height).
  ///
  /// @return The area.
  int GetArea() const {
    return width() * height();
  }

  /// SetSize() sets the value of width and height.
  ///
  /// @param[in] w A new width value.
  /// @param[in] h A new height value.
  void SetSize(int w, int h) {
    set_width(w);
    set_height(h);
  }

  /// Enlarge() enlarges the size of an object.
  ///
  /// @param[in] w A width to add the current width.
  /// @param[in] h A height to add to the current height.
  void Enlarge(int w, int h) {
    set_width(width() + w);
    set_height(height() + h);
  }

  /// IsEmpty() determines if the size is zero.
  ///
  /// @return true if the size is zero.
  bool IsEmpty() const {
    // Size doesn't allow negative dimensions, so testing for 0 is enough.
    return (width() == 0) || (height() == 0);
  }

 private:
  PP_Size size_;
};

/// A size of an object based on width and height.
class FloatSize {
 public:

  /// The default constructor. Initializes the width and height to 0.0f.
  FloatSize() {
    size_.width = 0.0f;
    size_.height = 0.0f;
  }

  /// A constructor accepting a pointer to a <code>PP_FloatSize</code> and
  /// converting the <code>PP_FloatSize</code> to a <code>FloatSize</code>.
  /// This is an implicit conversion constructor.
  ///
  /// @param[in] s A pointer to a <code>PP_FloatSize</code>.
  FloatSize(const PP_FloatSize& s) {  // Implicit.
    // Want the >= 0 checking of the setter.
    set_width(s.width);
    set_height(s.height);
  }

  /// A constructor accepting two float values for width and height and
  /// converting them to a <code>FloatSize</code>.
  ///
  /// @param[in] w An float value representing a width.
  /// @param[in] h An float value representing a height.
  FloatSize(float w, float h) {
    // Want the >= 0.0f checking of the setter.
    set_width(w);
    set_height(h);
  }

  /// Destructor.
  ~FloatSize() {
  }

  /// PP_FloatSize() allows implicit conversion of a <code>FloatSize</code> to a
  /// <code>PP_FloatSize</code>.
  ///
  /// @return A Size.
  operator PP_FloatSize() {
    return size_;
  }

  /// Getter function for returning the internal <code>PP_FloatSize</code>
  /// struct.
  ///
  /// @return A const reference to the internal <code>PP_FloatSize</code>
  /// struct.
  const PP_FloatSize& pp_float_size() const {
    return size_;
  }

  /// Getter function for returning the internal <code>PP_FloatSize</code>
  /// struct.
  ///
  /// @return A mutable reference to the <code>PP_FloatSize</code> struct.
  PP_FloatSize& pp_float_size() {
    return size_;
  }

  /// Getter function for returning the value of width.
  ///
  /// @return The value of width for this <code>FloatSize</code>.
  float width() const {
    return size_.width;
  }

  /// Setter function for setting the value of width.
  ///
  /// @param[in] w A new width value.
  void set_width(float w) {
    if (w < 0.0f) {
      PP_DCHECK(w >= 0.0f);
      w = 0.0f;
    }
    size_.width = w;
  }

  /// Getter function for returning the value of height.
  ///
  /// @return The value of height for this <code>FloatSize</code>.
  float height() const {
    return size_.height;
  }

  /// Setter function for setting the value of height.
  ///
  /// @param[in] h A new height value.
  void set_height(float h) {
    if (h < 0.0f) {
      PP_DCHECK(h >= 0.0f);
      h = 0.0f;
    }
    size_.height = h;
  }

  /// GetArea() determines the area (width * height).
  ///
  /// @return The area.
  float GetArea() const {
    return width() * height();
  }

  /// SetSize() sets the value of width and height.
  ///
  /// @param[in] w A new width value.
  /// @param[in] h A new height value.
  void SetSize(float w, float h) {
    set_width(w);
    set_height(h);
  }

  /// Enlarge() enlarges the size of an object.
  ///
  /// @param[in] w A width to add the current width.
  /// @param[in] h A height to add to the current height.
  void Enlarge(float w, float h) {
    set_width(width() + w);
    set_height(height() + h);
  }

  /// IsEmpty() determines if the size is zero.
  ///
  /// @return true if the size is zero.
  bool IsEmpty() const {
    // Size doesn't allow negative dimensions, so testing for 0.0f is enough.
    return (width() == 0.0f) || (height() == 0.0f);
  }

 private:
  PP_FloatSize size_;
};

}  // namespace pp

/// This function determines whether the width and height values of two sizes
/// are equal.
///
/// @param[in] lhs The <code>Size</code> on the left-hand side of the equation.
/// @param[in] rhs The <code>Size</code> on the right-hand side of the
/// equation.
///
/// @return true if they are equal, false if unequal.
inline bool operator==(const pp::Size& lhs, const pp::Size& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

/// This function determines whether two <code>Sizes</code> are not equal.
///
/// @param[in] lhs The <code>Size</code> on the left-hand side of the equation.
/// @param[in] rhs The <code>Size</code> on the right-hand side of the equation.
///
/// @return true if the <code>Size</code> of lhs are equal to the
/// <code>Size</code> of rhs, otherwise false.
inline bool operator!=(const pp::Size& lhs, const pp::Size& rhs) {
  return !(lhs == rhs);
}

/// This function determines whether the width and height values of two sizes
/// are equal.
///
/// @param[in] lhs The <code>FloatSize</code> on the left-hand side of the
/// equation.
/// @param[in] rhs The <code>FloatSize</code> on the right-hand side of the
/// equation.
///
/// @return true if they are equal, false if unequal.
inline bool operator==(const pp::FloatSize& lhs, const pp::FloatSize& rhs) {
  return lhs.width() == rhs.width() && lhs.height() == rhs.height();
}

/// This function determines whether two <code>FloatSizes</code> are not equal.
///
/// @param[in] lhs The <code>FloatSize</code> on the left-hand side of the
/// equation.
/// @param[in] rhs The <code>FloatSize</code> on the right-hand side of the
/// equation.
///
/// @return true if the <code>FloatSize</code> of lhs are equal to the
/// <code>FloatSize</code> of rhs, otherwise false.
inline bool operator!=(const pp::FloatSize& lhs, const pp::FloatSize& rhs) {
  return !(lhs == rhs);
}

#endif  // PPAPI_CPP_SIZE_H_

