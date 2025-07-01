// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/shared_impl/array_writer.h"

#include <stddef.h>

#include <algorithm>

#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

ArrayWriter::ArrayWriter() { Reset(); }

ArrayWriter::ArrayWriter(const PP_ArrayOutput& output)
    : pp_array_output_(output) {}

ArrayWriter::~ArrayWriter() {}

void ArrayWriter::Reset() {
  pp_array_output_.GetDataBuffer = NULL;
  pp_array_output_.user_data = NULL;
}

bool ArrayWriter::StoreResourceVector(
    const std::vector<scoped_refptr<Resource> >& input) {
  // Always call the alloc function, even on 0 array size.
  void* dest =
      pp_array_output_.GetDataBuffer(pp_array_output_.user_data,
                                     static_cast<uint32_t>(input.size()),
                                     sizeof(PP_Resource));

  // Regardless of success, we clear the output to prevent future calls on
  // this same output object.
  Reset();

  if (input.empty())
    return true;  // Allow plugin to return NULL on 0 elements.
  if (!dest)
    return false;

  // Convert to PP_Resources.
  PP_Resource* dest_resources = static_cast<PP_Resource*>(dest);
  for (size_t i = 0; i < input.size(); i++)
    dest_resources[i] = input[i]->GetReference();
  return true;
}

bool ArrayWriter::StoreResourceVector(const std::vector<PP_Resource>& input) {
  // Always call the alloc function, even on 0 array size.
  void* dest =
      pp_array_output_.GetDataBuffer(pp_array_output_.user_data,
                                     static_cast<uint32_t>(input.size()),
                                     sizeof(PP_Resource));

  // Regardless of success, we clear the output to prevent future calls on
  // this same output object.
  Reset();

  if (input.empty())
    return true;  // Allow plugin to return NULL on 0 elements.
  if (!dest) {
    // Free the resources.
    for (size_t i = 0; i < input.size(); i++)
      PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(input[i]);
    return false;
  }

  std::copy(input.begin(), input.end(), static_cast<PP_Resource*>(dest));
  return true;
}

bool ArrayWriter::StoreVarVector(
    const std::vector<scoped_refptr<Var> >& input) {
  // Always call the alloc function, even on 0 array size.
  void* dest =
      pp_array_output_.GetDataBuffer(pp_array_output_.user_data,
                                     static_cast<uint32_t>(input.size()),
                                     sizeof(PP_Var));

  // Regardless of success, we clear the output to prevent future calls on
  // this same output object.
  Reset();

  if (input.empty())
    return true;  // Allow plugin to return NULL on 0 elements.
  if (!dest)
    return false;

  // Convert to PP_Vars.
  PP_Var* dest_vars = static_cast<PP_Var*>(dest);
  for (size_t i = 0; i < input.size(); i++)
    dest_vars[i] = input[i]->GetPPVar();
  return true;
}

bool ArrayWriter::StoreVarVector(const std::vector<PP_Var>& input) {
  // Always call the alloc function, even on 0 array size.
  void* dest =
      pp_array_output_.GetDataBuffer(pp_array_output_.user_data,
                                     static_cast<uint32_t>(input.size()),
                                     sizeof(PP_Var));

  // Regardless of success, we clear the output to prevent future calls on
  // this same output object.
  Reset();

  if (input.empty())
    return true;  // Allow plugin to return NULL on 0 elements.
  if (!dest) {
    // Free the vars.
    for (size_t i = 0; i < input.size(); i++)
      PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(input[i]);
    return false;
  }

  std::copy(input.begin(), input.end(), static_cast<PP_Var*>(dest));
  return true;
}

}  // namespace ppapi
