// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_ARRAY_WRITER_H_
#define PPAPI_SHARED_IMPL_ARRAY_WRITER_H_

#include <stdint.h>
#include <string.h>

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class Resource;
class Var;

// Holds a PP_ArrayWriter and provides helper functions for writing arrays
// to it. It also handles 0-initialization of the raw C struct and attempts
// to prevent you from writing the array twice.
class PPAPI_SHARED_EXPORT ArrayWriter {
 public:
  ArrayWriter();  // Creates an is_null() object
  ArrayWriter(const PP_ArrayOutput& output);

  ArrayWriter(const ArrayWriter&) = delete;
  ArrayWriter& operator=(const ArrayWriter&) = delete;

  ~ArrayWriter();

  bool is_valid() const { return !!pp_array_output_.GetDataBuffer; }
  bool is_null() const { return !is_valid(); }

  void set_pp_array_output(const PP_ArrayOutput& output) {
    pp_array_output_ = output;
  }

  // Sets the array output back to its is_null() state.
  void Reset();

  // StoreArray() and StoreVector() copy the given array/vector of data to the
  // plugin output array.
  //
  // Returns true on success, false if the plugin reported allocation failure.
  // In either case, the object will become is_null() immediately after the
  // call since one output function should only be issued once.
  //
  // THIS IS DESIGNED FOR POD ONLY. For the case of resources, for example, we
  // want to transfer a reference only on success. Likewise, if you have a
  // structure of PP_Vars or a struct that contains a PP_Resource, we need to
  // make sure that the right thing happens with the ref on success and failure.
  template <typename T>
  bool StoreArray(const T* input, uint32_t count) {
    // Always call the alloc function, even on 0 array size.
    void* dest = pp_array_output_.GetDataBuffer(
        pp_array_output_.user_data, count, sizeof(T));

    // Regardless of success, we clear the output to prevent future calls on
    // this same output object.
    Reset();

    if (count == 0)
      return true;  // Allow plugin to return NULL on 0 elements.
    if (!dest)
      return false;

    if (input)
      memcpy(dest, input, sizeof(T) * count);
    return true;
  }

  // Copies the given array/vector of data to the plugin output array.  See
  // comment of StoreArray() for detail.
  template <typename T>
  bool StoreVector(const std::vector<T>& input) {
    return StoreArray(input.size() ? &input[0] : NULL,
                      static_cast<uint32_t>(input.size()));
  }

  // Stores the given vector of resources as PP_Resources to the output vector,
  // adding one reference to each.
  //
  // On failure this returns false, nothing will be copied, and the resource
  // refcounts will be unchanged. In either case, the object will become
  // is_null() immediately after the call since one output function should only
  // be issued once.
  //
  // Note: potentially this could be a template in case you have a vector of
  // FileRef objects, for example. However, this saves code since there's only
  // one instantiation and is sufficient for now.
  bool StoreResourceVector(const std::vector<scoped_refptr<Resource> >& input);

  // Like the above version but takes an array of AddRef'ed PP_Resources. On
  // storage failure, this will release each resource.
  bool StoreResourceVector(const std::vector<PP_Resource>& input);

  // Stores the given vector of vars as PP_Vars to the output vector,
  // adding one reference to each.
  //
  // On failure this returns false, nothing will be copied, and the var
  // refcounts will be unchanged. In either case, the object will become
  // is_null() immediately after the call since one output function should only
  // be issued once.
  bool StoreVarVector(const std::vector<scoped_refptr<Var> >& input);

  // Like the above version but takes an array of AddRef'ed PP_Vars. On
  // storage failure, this will release each var.
  bool StoreVarVector(const std::vector<PP_Var>& input);

 private:
  PP_ArrayOutput pp_array_output_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_ARRAY_WRITER_H_
