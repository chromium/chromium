// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_INSTANCE_HANDLE_H_
#define PPAPI_CPP_INSTANCE_HANDLE_H_

#include "ppapi/c/pp_instance.h"


/// @file
/// This file defines an instance handle used to identify an instance in a
/// constructor for a resource.
namespace pp {

class Instance;

/// An instance handle identifies an instance in a constructor for a resource.
/// This class solves two different problems:
///
/// 1. A pp::Instance object's lifetime is managed by the system on the main
/// pepper thread of the module. This means that it may get destroyed at any
/// time based on something that happens on the web page. Therefore, it's not
/// safe to refer to a <code>pp::Instance</code> object on a background thread.
/// Instead, we need to pass some kind of identifier to resource constructors
/// so that they may safely be used on background threads. If the instance
/// becomes invalid, the resource creation will fail on the background thread,
/// but it won't crash.
///
/// 2. <code>PP_Instance</code> would be a good identifier to use for this case.
/// However, using <code>PP_Instance</code> in the constructor to resources is
/// problematic because it is just a typedef for an integer, as is a
/// <code>PP_Resource</code>. Many resources have alternate constructors that
/// just take an existing <code>PP_Resource</code>, so the constructors would
/// be ambiguous. Having this wrapper around a <code>PP_Instance</code>
/// prevents this ambiguity, and also provides a nice place to consolidate an
/// implicit conversion from <code>pp::Instance*</code> for prettier code on
/// the main thread (you can just pass "this" to resource constructors in your
/// instance objects).
///
/// You should always pass an <code>InstanceHandle</code> to background threads
/// instead of a <code>pp::Instance</code>, and use them in resource
/// constructors and code that may be used from background threads.
class InstanceHandle {
 public:
  /// Implicit constructor for converting a <code>pp::Instance</code> to an
  /// instance handle.
  ///
  /// @param[in] instance The instance with which this
  /// <code>InstanceHandle</code> will be associated.
  InstanceHandle(Instance* instance);

  /// This constructor explicitly converts a <code>PP_Instance</code> to an
  /// instance handle. This should not be implicit because it can make some
  /// resource constructors ambiguous. <code>PP_Instance</code> is just a
  /// typedef for an integer, as is <code>PP_Resource</code>, so the compiler
  /// can get confused between the two.
  ///
  /// @param[in] pp_instance The instance with which this
  /// <code>InstanceHandle</code> will be associated.
  explicit InstanceHandle(PP_Instance pp_instance)
      : pp_instance_(pp_instance) {}

  /// The pp_instance() function returns the <code>PP_Instance</code>.
  ///
  /// @return A <code>PP_Instance</code> internal instance handle.
  PP_Instance pp_instance() const { return pp_instance_; }

 private:
  PP_Instance pp_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_INSTANCE_HANDLE_H_
