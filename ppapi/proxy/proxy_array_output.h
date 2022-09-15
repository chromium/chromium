// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PROXY_ARRAY_OUTPUT_H_
#define PPAPI_PROXY_PROXY_ARRAY_OUTPUT_H_

#include <stdint.h>

#include <vector>

#include "base/check.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_array_output.h"

// Like ppapi/cpp/array_output.h file in the C++ wrappers but for use in the
// proxy where we can't link to the C++ wrappers. This also adds a refcounted
// version.
//
// Use ArrayOutputAdapter when calling a function that synchronously returns
// an array of data. Use RefCountedArrayOutputAdapterWithStorage for
// asynchronous returns:
//
// void OnCallbackComplete(
//     int32_t result,
//     scoped_refptr<RefCountedArrayOutputAdapter<PP_Resource> > output) {
//   // Vector is in output->output().
// }
//
// void ScheduleCallback() {
//   base::scoped_refptr<RefCountedArrayOutputAdapter<PP_Resource> > output;
//
//   callback = factory.NewOptionalCallback(&OnCallbackComplete, output);
//   DoSomethingAsynchronously(output->pp_array_output(),
//                             callback.pp_completion_callback());
//   ...
namespace ppapi {
namespace proxy {

// Non-templatized base class for the array output conversion. It provides the
// C implementation of a PP_ArrayOutput whose callback function is implemented
// as a virtual call on a derived class. Do not use directly, use one of the
// derived classes below.
class ArrayOutputAdapterBase {
 public:
  ArrayOutputAdapterBase() {
    pp_array_output_.GetDataBuffer =
        &ArrayOutputAdapterBase::GetDataBufferThunk;
    pp_array_output_.user_data = this;
  }
  virtual ~ArrayOutputAdapterBase() {}

  const PP_ArrayOutput& pp_array_output() { return pp_array_output_; }

 protected:
  virtual void* GetDataBuffer(uint32_t element_count,
                              uint32_t element_size) = 0;

 private:
  static void* GetDataBufferThunk(void* user_data,
                                  uint32_t element_count,
                                  uint32_t element_size);

  PP_ArrayOutput pp_array_output_;

  // Disallow copying and assignment. This will do the wrong thing for most
  // subclasses.
  ArrayOutputAdapterBase(const ArrayOutputAdapterBase&);
  ArrayOutputAdapterBase& operator=(const ArrayOutputAdapterBase&);
};

// This adapter provides functionality for implementing a PP_ArrayOutput
// structure as writing to a given vector object.
//
// This is generally used internally in the C++ wrapper objects to
// write into an output parameter supplied by the plugin. If the element size
// that the browser is writing does not match the size of the type we're using
// this will assert and return NULL (which will cause the browser to fail the
// call).
//
// Example that allows the browser to write into a given vector:
//   void DoFoo(std::vector<int>* results) {
//     ArrayOutputAdapter<int> adapter(results);
//     ppb_foo->DoFoo(adapter.pp_array_output());
//   }
template<typename T>
class ArrayOutputAdapter : public ArrayOutputAdapterBase {
 public:
  ArrayOutputAdapter(std::vector<T>* output) : output_(output) {}

 protected:
  // Two-step init for the "with storage" version below.
  ArrayOutputAdapter() : output_(NULL) {}
  void set_output(std::vector<T>* output) { output_ = output; }

  // ArrayOutputAdapterBase implementation.
  virtual void* GetDataBuffer(uint32_t element_count, uint32_t element_size) {
    DCHECK(element_size == sizeof(T));
    if (element_count == 0 || element_size != sizeof(T))
      return NULL;
    output_->resize(element_count);
    return &(*output_)[0];
  }

 private:
  std::vector<T>* output_;
};

template<typename T>
class ArrayOutputAdapterWithStorage : public ArrayOutputAdapter<T> {
 public:
  ArrayOutputAdapterWithStorage() {
    // Note: "this->" is required due to two-phase name lookup where it isn't
    // allowed to look in the base class during parsing.
    this->set_output(&output_storage_);
  }

  std::vector<T>& output() { return output_storage_; }

 private:
  std::vector<T> output_storage_;
};

// A reference counted version of ArrayOutputAdapterWithStorage. Since it
// doesn't make much sense to heap-allocate one without storage, we don't
// call it "with storage" to keep the name length under control.
template<typename T>
class RefCountedArrayOutputAdapter
    : public ArrayOutputAdapterWithStorage<T>,
      public base::RefCounted<RefCountedArrayOutputAdapter<T> > {
 public:
  RefCountedArrayOutputAdapter()
      : ArrayOutputAdapterWithStorage<T>() {
  }
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PROXY_ARRAY_OUTPUT_H_
