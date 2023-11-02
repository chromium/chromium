// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VAR_ARRAY_H_
#define PPAPI_CPP_VAR_ARRAY_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/var.h"

/// @file
/// This file defines the API for interacting with array vars.

namespace pp {

class VarArray : public Var {
 public:
  /// Constructs a new array var.
  VarArray();

  /// Constructs a <code>VarArray</code> given a var for which is_array() is
  /// true. This will refer to the same array var, but allow you to access
  /// methods specific to arrays.
  ///
  /// @param[in] var An array var.
  explicit VarArray(const Var& var);

  /// Constructs a <code>VarArray</code> given a <code>PP_Var</code> of type
  /// PP_VARTYPE_ARRAY.
  ///
  /// @param[in] var A <code>PP_Var</code> of type PP_VARTYPE_ARRAY.
  explicit VarArray(const PP_Var& var);

  /// Copy constructor.
  VarArray(const VarArray& other);

  virtual ~VarArray();

  /// Assignment operator.
  VarArray& operator=(const VarArray& other);

  /// The <code>Var</code> assignment operator is overridden here so that we can
  /// check for assigning a non-array var to a <code>VarArray</code>.
  ///
  /// @param[in] other The array var to be assigned.
  ///
  /// @return The resulting <code>VarArray</code> (as a <code>Var</code>&).
  virtual Var& operator=(const Var& other);

  /// Gets an element from the array.
  ///
  /// @param[in] index An index indicating which element to return.
  ///
  /// @return The element at the specified position. If <code>index</code> is
  /// larger than or equal to the array length, an undefined var is returned.
  Var Get(uint32_t index) const;

  /// Sets the value of an element in the array.
  ///
  /// @param[in] index An index indicating which element to modify. If
  /// <code>index</code> is larger than or equal to the array length, the length
  /// is updated to be <code>index</code> + 1. Any position in the array that
  /// hasn't been set before is set to undefined, i.e., <code>PP_Var</code> of
  /// type <code>PP_VARTYPE_UNDEFINED</code>.
  /// @param[in] value The value to set.
  ///
  /// @return A <code>bool</code> indicating whether the operation succeeds.
  bool Set(uint32_t index, const Var& value);

  /// Gets the array length.
  ///
  /// @return The array length.
  uint32_t GetLength() const;

  /// Sets the array length.
  ///
  /// @param[in] length The new array length. If <code>length</code> is smaller
  /// than its current value, the array is truncated to the new length; any
  /// elements that no longer fit are removed. If <code>length</code> is larger
  /// than its current value, undefined vars are appended to increase the array
  /// to the specified length.
  ///
  /// @return A <code>bool</code> indicating whether the operation succeeds.
  bool SetLength(uint32_t length);
};

}  // namespace pp

#endif  // PPAPI_CPP_VAR_ARRAY_H_
