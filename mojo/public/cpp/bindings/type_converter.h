// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TYPE_CONVERTER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TYPE_CONVERTER_H_

#include <stdint.h>

#include <concepts>
#include <memory>
#include <type_traits>

#include "base/types/to_address.h"

namespace mojo {

// NOTE: When possible, please consider using StructTraits / UnionTraits /
// EnumTraits / ArrayTraits / MapTraits / StringTraits if you would like to
// convert between custom types and the wire format of mojom types. The use of
// TypeConverter should be limited as much as possible: ideally, only use it in
// renderers, e.g., for Blink IDL and Oilpan types.
//
// Specialize the following class:
//   template <typename T, typename U> struct TypeConverter;
// to perform type conversion for Mojom-defined structs and arrays. Here, T is
// the target type; U is the input type.
//
// Specializations should implement the following interfaces:
//   namespace mojo {
//   template <>
//   struct TypeConverter<X, Y> {
//     static X Convert(const Y& input);
//   };
//   template <>
//   struct TypeConverter<Y, X> {
//     static Y Convert(const X& input);
//   };
//   }
//
// EXAMPLE:
//
// Suppose you have the following Mojom-defined struct:
//
//   module geometry {
//   struct Point {
//     int32_t x;
//     int32_t y;
//   };
//   }
//
// Now, imagine you wanted to write a TypeConverter specialization for
// gfx::Point. It might look like this:
//
//   namespace mojo {
//   template <>
//   struct TypeConverter<geometry::PointPtr, gfx::Point> {
//     static geometry::PointPtr Convert(const gfx::Point& input) {
//       geometry::PointPtr result;
//       result->x = input.x();
//       result->y = input.y();
//       return result;
//     }
//   };
//   template <>
//   struct TypeConverter<gfx::Point, geometry::PointPtr> {
//     static gfx::Point Convert(const geometry::PointPtr& input) {
//       return input ? gfx::Point(input->x, input->y) : gfx::Point();
//     }
//   };
//   }
//
// With the above TypeConverter defined, it is possible to write code like this:
//
//   void AcceptPoint(const geometry::PointPtr& input) {
//     // With an explicit cast using the .To<> method.
//     gfx::Point pt = input.To<gfx::Point>();
//
//     // With an explicit cast using the static From() method.
//     geometry::PointPtr output = geometry::Point::From(pt);
//
//     // Inferring the input type using the ConvertTo helper function.
//     gfx::Point pt2 = ConvertTo<gfx::Point>(input);
//   }
//
template <typename T, typename U>
struct TypeConverter;

// The following helper functions are useful shorthand. The compiler can infer
// the input type, so you can write:
//   OutputType out = ConvertTo<OutputType>(input);
template <typename T, typename U>
  requires requires(U* obj) {
    { TypeConverter<T, std::remove_cv_t<U>*>::Convert(obj) } -> std::same_as<T>;
  }
inline T ConvertTo(U* obj) {
  return TypeConverter<T, std::remove_cv_t<U>*>::Convert(obj);
}

template <typename T, typename U>
  requires requires(const U& obj) {
    !std::is_pointer_v<U>;
    { mojo::ConvertTo<T>(base::to_address(obj)) } -> std::same_as<T>;
  }
inline T ConvertTo(const U& obj) {
  return mojo::ConvertTo<T>(base::to_address(obj));
}

template <typename T, typename U>
  requires requires(const U& obj) {
    !std::is_pointer_v<U>;
    TypeConverter<T, U>::Convert(obj);
  }
inline T ConvertTo(const U& obj) {
  return TypeConverter<T, U>::Convert(obj);
}

template <typename T>
struct TypeConverter<T, T> {
  static T Convert(const T& obj) { return obj; }
};

namespace internal {

template <typename Vec>
using VecValueType = typename Vec::value_type;

template <typename Vec>
using VecPtrLikeUnderlyingValueType =
    std::pointer_traits<VecValueType<Vec>>::element_type;

}  // namespace internal

// Generic specialization for converting between different vector-like
// containers.
template <typename OutVec, typename InVec>
  requires requires(const InVec& in, OutVec& out) {
    out.reserve(in.size());
    out.push_back(mojo::ConvertTo<internal::VecValueType<OutVec>>(*in.begin()));
  }
struct TypeConverter<OutVec, InVec> {
  static OutVec Convert(const InVec& in) {
    OutVec out;
    out.reserve(in.size());
    for (const auto& obj : in) {
      out.push_back(mojo::ConvertTo<internal::VecValueType<OutVec>>(obj));
    }
    return out;
  }
};

// Specialization for converting from Vector<U> to Vector<PtrLike<T>> with only
// TypeConverter<T*, U> defined.
template <typename OutVec, typename InVec>
  requires requires(const InVec& in, OutVec& out) {
    out.reserve(in.size());
    out.emplace_back(
        mojo::ConvertTo<internal::VecPtrLikeUnderlyingValueType<OutVec>*>(
            *in.begin()));
  }
struct TypeConverter<OutVec, InVec> {
  static OutVec Convert(const InVec& in) {
    OutVec out;
    out.reserve(in.size());
    for (const auto& obj : in) {
      out.emplace_back(
          mojo::ConvertTo<internal::VecPtrLikeUnderlyingValueType<OutVec>*>(
              obj));
    }
    return out;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TYPE_CONVERTER_H_
