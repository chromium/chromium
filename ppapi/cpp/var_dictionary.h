// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VAR_DICTIONARY_H_
#define PPAPI_CPP_VAR_DICTIONARY_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array.h"

/// @file
/// This file defines the API for interacting with dictionary vars.

namespace pp {

class VarDictionary : public Var {
 public:
  /// Constructs a new dictionary var.
  VarDictionary();

  /// Constructs a <code>VarDictionary</code> given a var for which
  /// is_dictionary() is true. This will refer to the same dictionary var, but
  /// allow you to access methods specific to dictionary.
  ///
  /// @param[in] var A dictionary var.
  explicit VarDictionary(const Var& var);

  /// Constructs a <code>VarDictionary</code> given a <code>PP_Var</code>
  /// of type PP_VARTYPE_DICTIONARY.
  ///
  /// @param[in] var A <code>PP_Var</code> of type PP_VARTYPE_DICTIONARY.
  explicit VarDictionary(const PP_Var& var);

  /// Copy constructor.
  VarDictionary(const VarDictionary& other);

  virtual ~VarDictionary();

  /// Assignment operator.
  VarDictionary& operator=(const VarDictionary& other);

  /// The <code>Var</code> assignment operator is overridden here so that we can
  /// check for assigning a non-dictionary var to a
  /// <code>VarDictionary</code>.
  ///
  /// @param[in] other The dictionary var to be assigned.
  ///
  /// @return The resulting <code>VarDictionary</code> (as a
  /// <code>Var</code>&).
  virtual Var& operator=(const Var& other);

  /// Gets the value associated with the specified key.
  ///
  /// @param[in] key A string var.
  ///
  /// @return The value that is associated with <code>key</code>. If
  /// <code>key</code> is not a string var, or it doesn't exist in the
  /// dictionary, an undefined var is returned.
  Var Get(const Var& key) const;

  /// Sets the value associated with the specified key.
  ///
  /// @param[in] key A string var. If this key hasn't existed in the dictionary,
  /// it is added and associated with <code>value</code>; otherwise, the
  /// previous value is replaced with <code>value</code>.
  /// @param[in] value The value to set.
  ///
  /// @return A <code>bool</code> indicating whether the operation succeeds.
  bool Set(const Var& key, const Var& value);

  /// Deletes the specified key and its associated value, if the key exists.
  ///
  /// @param[in] key A string var.
  void Delete(const Var& key);

  /// Checks whether a key exists.
  ///
  /// @param[in] key A string var.
  ///
  /// @return A <code>bool</code> indicating whether the key exists.
  bool HasKey(const Var& key) const;

  /// Gets all the keys in the dictionary.
  ///
  /// @return An array var which contains all the keys of the dictionary.
  /// The elements are string vars. Returns an empty array var if failed.
  VarArray GetKeys() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_VAR_DICTIONARY_H_
