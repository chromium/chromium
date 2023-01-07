// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_ARRAY_OUTPUT_H_
#define PPAPI_CPP_ARRAY_OUTPUT_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "ppapi/c/pp_array_output.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/var.h"

namespace pp {

// Converts the given array of PP_Resources into an array of the requested
// C++ resource types, passing ownership of a reference in the process.
//
// This is used to convert output arrays of resources that the browser has
// generated into the more convenient C++ wrappers for those resources. The
// initial "PassRef" parameter is there to emphasize what happens to the
// reference count of the input resource and to match the resource constructors
// that look the same.
template<typename ResourceObjectType>
inline void ConvertPPResourceArrayToObjects(
    PassRef,
    const std::vector<PP_Resource>& input,
    std::vector<ResourceObjectType>* output) {
  output->resize(0);
  output->reserve(input.size());
  for (size_t i = 0; i < input.size(); i++)
    output->push_back(ResourceObjectType(PASS_REF, input[i]));
}

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
    if (element_count == 0)
      return NULL;
    PP_DCHECK(element_size == sizeof(T));
    if (element_size != sizeof(T))
      return NULL;
    output_->resize(element_count);
    return &(*output_)[0];
  }

 private:
  std::vector<T>* output_;
};

// This adapter provides functionality for implementing a PP_ArrayOutput
// structure as writing resources to a given vector object.
//
// When returning an array of resources, the browser will write PP_Resources
// via a PP_ArrayOutput. This code will automatically convert the PP_Resources
// to the given wrapper type, (as long as that wrapper type supports the
// correct constructor). The ownership of the resources that the browser passed
// to us will be transferred to the C++ wrapper object.
//
// Conversion of the PP_Resources to the C++ wrapper object occurs in the
// destructor. This object is intended to be used on the stack in a C++ wrapper
// object for a call.
//
// Example:
//   void GetFiles(std::vector<pp::FileRef>* results) {
//     ResourceArrayOutputAdapter<pp::FileRef> adapter(results);
//     ppb_foo->DoFoo(adapter.pp_array_output());
//   }
template<typename T>
class ResourceArrayOutputAdapter : public ArrayOutputAdapterBase {
 public:
  explicit ResourceArrayOutputAdapter(std::vector<T>* output)
      : output_(output) {
    output_->resize(0);
  }
  virtual ~ResourceArrayOutputAdapter() {
    ConvertPPResourceArrayToObjects(PASS_REF, intermediate_output_, output_);
  }

 protected:
  // Two-step init for the "with storage" version below.
  ResourceArrayOutputAdapter() : output_(NULL) {}
  void set_output(T* output) { output_ = output; }

  // ArrayOutputAdapterBase implementation.
  virtual void* GetDataBuffer(uint32_t element_count,
                              uint32_t element_size) {
    if (element_count == 0)
      return NULL;
    PP_DCHECK(element_size == sizeof(PP_Resource));
    if (element_size != sizeof(PP_Resource))
      return NULL;
    intermediate_output_.resize(element_count);
    return &intermediate_output_[0];
  }

 private:
  std::vector<PP_Resource> intermediate_output_;
  std::vector<T>* output_;
};

// This adapter is like the ArrayOutputAdapter except that it also contains
// the underlying std::vector that will be populated (rather than writing it to
// an object passed into the constructor).
//
// This is used by the CompletionCallbackFactory system to collect the output
// parameters from an async function call. The collected data is then passed to
// the plugins callback function.
//
// You can also use it directly if you want to have an array output and aren't
// using the CompletionCallbackFactory. For example, if you're calling a
// PPAPI function DoFoo that takes a PP_OutputArray that is supposed to be
// writing integers, do this:
//
//    ArrayOutputAdapterWithStorage<int> adapter;
//    ppb_foo->DoFoo(adapter.pp_output_array());
//    const std::vector<int>& result = adapter.output();
template<typename T>
class ArrayOutputAdapterWithStorage : public ArrayOutputAdapter<T> {
 public:
  ArrayOutputAdapterWithStorage() {
    this->set_output(&output_storage_);
  }

  std::vector<T>& output() { return output_storage_; }

 private:
  std::vector<T> output_storage_;
};

// This adapter is like the ArrayOutputAdapterWithStorage except this
// additionally converts PP_Var structs to pp::Var objects.
//
// You can also use it directly if you want to have an array output and aren't
// using the CompletionCallbackFactory. For example, if you're calling a
// PPAPI function GetVars that takes a PP_OutputArray that is supposed to be
// writing PP_Vars, do this:
//
//    VarArrayOutputAdapterWithStorage adapter;
//    ppb_foo->GetVars(adapter.pp_output_array());
//    const std::vector<pp::Var>& result = adapter.output().
//
// This one is non-inline since it's not templatized.
class VarArrayOutputAdapterWithStorage : public ArrayOutputAdapter<PP_Var> {
 public:
  VarArrayOutputAdapterWithStorage();
  virtual ~VarArrayOutputAdapterWithStorage();

  // Returns the final array of resource objects, converting the PP_Vars
  // written by the browser to pp::Var objects.
  //
  // This function should only be called once or we would end up converting
  // the array more than once, which would mess up the refcounting.
  std::vector<Var>& output();

 private:
  // The browser will write the PP_Vars into this array.
  std::vector<PP_Var> temp_storage_;

  // When asked for the output, the resources above will be converted to the
  // C++ resource objects in this array for passing to the calling code.
  std::vector<Var> output_storage_;
};

// This adapter is like the ArrayOutputAdapterWithStorage except this
// additionally converts PP_Resources to C++ wrapper objects of the given type.
//
// You can also use it directly if you want to have an array output and aren't
// using the CompletionCallbackFactory. For example, if you're calling a
// PPAPI function GetFiles that takes a PP_OutputArray that is supposed to be
// writing PP_Resources cooresponding to FileRefs, do this:
//
//    ResourceArrayOutputAdapterWithStorage<FileRef> adapter;
//    ppb_foo->GetFiles(adapter.pp_output_array());
//    std::vector<FileRef> result = adapter.output().
template<typename T>
class ResourceArrayOutputAdapterWithStorage
    : public ArrayOutputAdapter<PP_Resource> {
 public:
  ResourceArrayOutputAdapterWithStorage() {
    set_output(&temp_storage_);
  }

  virtual ~ResourceArrayOutputAdapterWithStorage() {
    if (!temp_storage_.empty()) {
      // An easy way to release the resource references held by this object.
      output();
    }
  }

  // Returns the final array of resource objects, converting the PP_Resources
  // written by the browser to resource objects.
  //
  // This function should only be called once or we would end up converting
  // the array more than once, which would mess up the refcounting.
  std::vector<T>& output() {
    PP_DCHECK(output_storage_.empty());

    ConvertPPResourceArrayToObjects(PASS_REF, temp_storage_, &output_storage_);
    temp_storage_.clear();
    return output_storage_;
  }

 private:
  // The browser will write the PP_Resources into this array.
  std::vector<PP_Resource> temp_storage_;

  // When asked for the output, the resources above will be converted to the
  // C++ resource objects in this array for passing to the calling code.
  std::vector<T> output_storage_;
};

}  // namespace pp

#endif  // PPAPI_CPP_ARRAY_OUTPUT_H_
