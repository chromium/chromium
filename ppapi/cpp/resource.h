// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_RESOURCE_H_
#define PPAPI_CPP_RESOURCE_H_

#include "ppapi/c/pp_resource.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/pass_ref.h"

/// @file
/// This file defines a <code>Resource</code> type representing data associated
/// with the module.
namespace pp {

class Var;

/// A reference counted module resource.
class Resource {
 public:
  /// The default constructor.
  Resource();

  /// A constructor for copying a resource.
  ///
  /// @param[in] other A <code>Resource</code>.
  Resource(const Resource& other);

  /// Destructor.
  virtual ~Resource();

  /// This function assigns one <code>Resource</code> to another
  /// <code>Resource</code>.
  ///
  /// @param[in] other A Resource.
  ///
  /// @return A Resource containing the assigned Resource.
  Resource& operator=(const Resource& other);

  /// This functions determines if this resource is invalid or
  /// uninitialized.
  ///
  /// @return true if this resource is invalid or uninitialized.
  bool is_null() const { return !pp_resource_; }

  PP_Resource pp_resource() const { return pp_resource_; }

  /// This function releases ownership of this resource and returns it to the
  /// caller.
  ///
  /// Note that the reference count on the resource is unchanged and the caller
  /// needs to release the resource.
  ///
  /// @return The detached <code>PP_Resource</code>.
  PP_Resource detach();

 protected:
  /// A constructor used when a <code>PP_Resource</code> is provided as a
  /// return value whose reference count we need to increment.
  ///
  /// @param[in] resource A <code>PP_Resource</code> corresponding to a
  /// resource.
  explicit Resource(PP_Resource resource);

  /// Constructor used when a <code>PP_Resource</code> already has a ref count
  /// assigned. Add additional refcount is not taken.
  Resource(PassRef, PP_Resource resource);

  /// PassRefFromConstructor is called by derived class' constructors to
  /// initialize this <code>Resource</code> with a <code>PP_Resource</code>
  /// that has already had its reference count incremented by
  /// <code>Core::AddRefResource</code>. It also assumes this object has no
  /// current resource.
  ///
  /// The intended usage of this function that the derived class constructor
  /// will call the default <code>Resource</code> constructor, then make a call
  /// to create a resource. It then wants to assign the new resource (which,
  /// since it was returned by the browser, already had its reference count
  /// increased).
  ///
  /// @param[in] resource A <code>PP_Resource</code> corresponding to a
  /// resource.
  void PassRefFromConstructor(PP_Resource resource);

  /// Sets this resource to null. This releases ownership of the resource.
  void Clear();

 private:
  friend class Var;

  PP_Resource pp_resource_;
};

}  // namespace pp

inline bool operator==(const pp::Resource& lhs, const pp::Resource& rhs) {
  return lhs.pp_resource() == rhs.pp_resource();
}

inline bool operator!=(const pp::Resource& lhs, const pp::Resource& rhs) {
  return !(lhs == rhs);
}

#endif // PPAPI_CPP_RESOURCE_H_
