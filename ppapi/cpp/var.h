// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VAR_H_
#define PPAPI_CPP_VAR_H_

#include <stdint.h>

#include <string>

#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the API for handling the passing of data types between
/// your module and the page.
namespace pp {

/// A generic type used for passing data types between the module and the page.
class Var {
 public:
  /// Special value passed to constructor to make <code>NULL</code>.
  struct Null {};

  /// Default constructor. Creates a <code>Var</code> of type
  /// <code>Undefined</code>.
  Var();

  /// A constructor used to create a <code>Var</code> of type <code>Null</code>.
  Var(Null);

  /// A constructor used to create a <code>Var</code> of type <code>Bool</code>.
  ///
  /// @param[in] b A boolean value.
  Var(bool b);

  /// A constructor used to create a 32 bit integer <code>Var</code>.
  ///
  /// @param[in] i A 32 bit integer value.
  Var(int32_t i);

  /// A constructor used to create a double value <code>Var</code>.
  ///
  /// @param[in] d A double value.
  Var(double d);

  /// A constructor used to create a UTF-8 character <code>Var</code>.
  Var(const char* utf8_str);  // Must be encoded in UTF-8.

  /// A constructor used to create a UTF-8 character <code>Var</code>.
  Var(const std::string& utf8_str);  // Must be encoded in UTF-8.

  /// A constructor used to create a resource <code>Var</code>.
  explicit Var(const pp::Resource& resource);

  /// A constructor used when you have received a <code>Var</code> as a return
  /// value that has had its reference count incremented for you.
  ///
  /// You will not normally need to use this constructor because
  /// the reference count will not normally be incremented for you.
  Var(PassRef, const PP_Var& var) {
    var_ = var;
    is_managed_ = true;
  }

  /// A constructor that increments the reference count.
  explicit Var(const PP_Var& var);

  struct DontManage {};

  /// This constructor is used when we've given a <code>PP_Var</code> as an
  /// input argument from somewhere and that reference is managing the
  /// reference count for us. The object will not have its reference count
  /// increased or decreased by this class instance.
  ///
  /// @param[in] var A <code>Var</code>.
  Var(DontManage, const PP_Var& var) {
    var_ = var;
    is_managed_ = false;
  }

  /// A constructor for copying a <code>Var</code>.
  Var(const Var& other);

  /// Destructor.
  virtual ~Var();

  /// This function assigns one <code>Var</code> to another <code>Var</code>.
  ///
  /// @param[in] other The <code>Var</code> to be assigned.
  ///
  /// @return A resulting <code>Var</code>.
  virtual Var& operator=(const Var& other);

  /// This function compares object identity (rather than value identity) for
  /// objects, dictionaries, and arrays
  ///
  /// @param[in] other The <code>Var</code> to be compared to this Var.
  ///
  /// @return true if the <code>other</code> <code>Var</code> is the same as
  /// this <code>Var</code>, otherwise false.
  bool operator==(const Var& other) const;

  /// This function determines if this <code>Var</code> is an undefined value.
  ///
  /// @return true if this <code>Var</code> is undefined, otherwise false.
  bool is_undefined() const { return var_.type == PP_VARTYPE_UNDEFINED; }

  /// This function determines if this <code>Var</code> is a null value.
  ///
  /// @return true if this <code>Var</code> is null, otherwise false.
  bool is_null() const { return var_.type == PP_VARTYPE_NULL; }

  /// This function determines if this <code>Var</code> is a bool value.
  ///
  /// @return true if this <code>Var</code> is a bool, otherwise false.
  bool is_bool() const { return var_.type == PP_VARTYPE_BOOL; }

  /// This function determines if this <code>Var</code> is a string value.
  ///
  /// @return true if this <code>Var</code> is a string, otherwise false.
  bool is_string() const { return var_.type == PP_VARTYPE_STRING; }

  /// This function determines if this <code>Var</code> is an object.
  ///
  /// @return true if this <code>Var</code> is an object, otherwise false.
  bool is_object() const { return var_.type == PP_VARTYPE_OBJECT; }

  /// This function determines if this <code>Var</code> is an array.
  ///
  /// @return true if this <code>Var</code> is an array, otherwise false.
  bool is_array() const { return var_.type == PP_VARTYPE_ARRAY; }

  /// This function determines if this <code>Var</code> is a dictionary.
  ///
  /// @return true if this <code>Var</code> is a dictionary, otherwise false.
  bool is_dictionary() const { return var_.type == PP_VARTYPE_DICTIONARY; }

  /// This function determines if this <code>Var</code> is a resource.
  ///
  /// @return true if this <code>Var</code> is a resource, otherwise false.
  bool is_resource() const { return var_.type == PP_VARTYPE_RESOURCE; }

  /// This function determines if this <code>Var</code> is an integer value.
  /// The <code>is_int</code> function returns the internal representation.
  /// The JavaScript runtime may convert between the two as needed, so the
  /// distinction may not be relevant in all cases (int is really an
  /// optimization inside the runtime). So most of the time, you will want
  /// to check is_number().
  ///
  /// @return true if this <code>Var</code> is an integer, otherwise false.
  bool is_int() const { return var_.type == PP_VARTYPE_INT32; }

  /// This function determines if this <code>Var</code> is a double value.
  /// The <code>is_double</code> function returns the internal representation.
  /// The JavaScript runtime may convert between the two as needed, so the
  /// distinction may not be relevant in all cases (int is really an
  /// optimization inside the runtime). So most of the time, you will want to
  /// check is_number().
  ///
  /// @return true if this <code>Var</code> is a double, otherwise false.
  bool is_double() const { return var_.type == PP_VARTYPE_DOUBLE; }

  /// This function determines if this <code>Var</code> is a number.
  ///
  /// @return true if this <code>Var</code> is an int32_t or double number,
  /// otherwise false.
  bool is_number() const {
    return var_.type == PP_VARTYPE_INT32 ||
           var_.type == PP_VARTYPE_DOUBLE;
  }

  /// This function determines if this <code>Var</code> is an ArrayBuffer.
  bool is_array_buffer() const { return var_.type == PP_VARTYPE_ARRAY_BUFFER; }

  /// AsBool() converts this <code>Var</code> to a bool. Assumes the
  /// internal representation is_bool(). If it's not, it will assert in debug
  /// mode, and return false.
  ///
  /// @return A bool version of this <code>Var</code>.
  bool AsBool() const;

  /// AsInt() converts this <code>Var</code> to an int32_t. This function
  /// is required because JavaScript doesn't have a concept of ints and doubles,
  /// only numbers. The distinction between the two is an optimization inside
  /// the compiler. Since converting from a double to an int may be lossy, if
  /// you care about the distinction, either always work in doubles, or check
  /// !is_double() before calling AsInt().
  ///
  /// These functions will assert in debug mode and return 0 if the internal
  /// representation is not is_number().
  ///
  /// @return An int32_t version of this <code>Var</code>.
  int32_t AsInt() const;

  /// AsDouble() converts this <code>Var</code> to a double. This function is
  /// necessary because JavaScript doesn't have a concept of ints and doubles,
  /// only numbers. The distinction between the two is an optimization inside
  /// the compiler. Since converting from a double to an int may be lossy, if
  /// you care about the distinction, either always work in doubles, or check
  /// !is_double() before calling AsInt().
  ///
  /// These functions will assert in debug mode and return 0 if the internal
  /// representation is not is_number().
  ///
  /// @return An double version of this <code>Var</code>.
  double AsDouble() const;

  /// AsString() converts this <code>Var</code> to a string. If this object is
  /// not a string, it will assert in debug mode, and return an empty string.
  ///
  /// @return A string version of this <code>Var</code>.
  std::string AsString() const;

  /// Gets the resource contained in the var. If this object is not a resource,
  /// it will assert in debug mode, and return a null resource.
  ///
  /// @return The <code>pp::Resource</code> that is contained in the var.
  pp::Resource AsResource() const;

  /// This function returns the internal <code>PP_Var</code>
  /// managed by this <code>Var</code> object.
  ///
  /// @return A const reference to a <code>PP_Var</code>.
  const PP_Var& pp_var() const {
    return var_;
  }

  /// Detach() detaches from the internal <code>PP_Var</code> of this
  /// object, keeping the reference count the same. This is used when returning
  /// a <code>PP_Var</code> from an API function where the caller expects the
  /// return value to have the reference count incremented for it.
  ///
  /// @return A detached version of this object without affecting the reference
  /// count.
  PP_Var Detach() {
    PP_Var ret = var_;
    var_ = PP_MakeUndefined();
    is_managed_ = true;
    return ret;
  }

  /// DebugString() returns a short description "Var<X>" that can be used for
  /// logging, where "X" is the underlying scalar or "UNDEFINED" or "OBJ" as
  /// it does not call into the browser to get the object description.
  ///
  /// @return A string displaying the value of this <code>Var</code>. This
  /// function is used for debugging.
  std::string DebugString() const;

  /// This class is used when calling the raw C PPAPI when using the C++
  /// <code>Var</code> as a possible NULL exception. This class will handle
  /// getting the address of the internal value out if it's non-NULL and
  /// fixing up the reference count.
  ///
  /// <strong>Warning:</strong> this will only work for things with exception
  /// semantics, i.e. that the value will not be changed if it's a
  /// non-undefined exception. Otherwise, this class will mess up the
  /// refcounting.
  ///
  /// This is a bit subtle:
  /// - If NULL is passed, we return NULL from get() and do nothing.
  ///
  /// - If a undefined value is passed, we return the address of a undefined
  ///   var from get and have the output value take ownership of that var.
  ///
  /// - If a non-undefined value is passed, we return the address of that var
  ///   from get, and nothing else should change.
  ///
  /// Example:
  ///   void FooBar(a, b, Var* exception = NULL) {
  ///     foo_interface->Bar(a, b, Var::OutException(exception).get());
  ///   }
  class OutException {
   public:
    /// A constructor.
    OutException(Var* v)
        : output_(v),
          originally_had_exception_(v && !v->is_undefined()) {
      if (output_) {
        temp_ = output_->var_;
      } else {
        temp_.padding = 0;
        temp_.type = PP_VARTYPE_UNDEFINED;
      }
    }

    /// Destructor.
    ~OutException() {
      if (output_ && !originally_had_exception_)
        *output_ = Var(PASS_REF, temp_);
    }

    PP_Var* get() {
      if (output_)
        return &temp_;
      return NULL;
    }

   private:
    Var* output_;
    bool originally_had_exception_;
    PP_Var temp_;
  };

 protected:
  PP_Var var_;

  // |is_managed_| indicates if the instance manages |var_|.
  // You need to check if |var_| is refcounted to call Release().
  bool is_managed_;

 private:
  // Prevent an arbitrary pointer argument from being implicitly converted to
  // a bool at Var construction. If somebody makes such a mistake, they will
  // get a compilation error.
  Var(void* non_scriptable_object_pointer);
};

}  // namespace pp

#endif  // PPAPI_CPP_VAR_H_
