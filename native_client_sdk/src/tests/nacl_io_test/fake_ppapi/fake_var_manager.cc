// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_var_manager.h"
#include "gtest/gtest.h"
#include "sdk_util/auto_lock.h"

FakeVarManager::FakeVarManager() : debug(false), next_id_(1) {}

FakeVarManager::~FakeVarManager() {
  // The ref counts for all vars should be zero.
  for (VarMap::const_iterator iter = var_map_.begin(); iter != var_map_.end();
       ++iter) {
    const FakeVarData& var_data = iter->second;
    EXPECT_EQ(0, var_data.ref_count) << "Non-zero refcount on "
                                     << Describe(var_data);
  }
}

FakeVarData* FakeVarManager::CreateVarData() {
  AUTO_LOCK(lock_);
  Id id = next_id_++;
  FakeVarData data;
  data.id = id;
  data.ref_count = 1;
  var_map_[id] = data;
  return &var_map_[id];
}

void FakeVarManager::AddRef(PP_Var var) {
  AUTO_LOCK(lock_);
  // From ppb_var.h:
  //   AddRef() adds a reference to the given var. If this is not a refcounted
  //   object, this function will do nothing so you can always call it no matter
  //   what the type.

  FakeVarData* var_data = GetVarData_Locked(var);
  if (!var_data)
    return;

  EXPECT_GT(var_data->ref_count, 0)
       << "AddRefing freed " << Describe(*var_data);
  var_data->ref_count++;
  if (debug)
    printf("AddRef of %s [new refcount=%d]\n",
           Describe(*var_data).c_str(),
           var_data->ref_count);
}

std::string FakeVarManager::Describe(const FakeVarData& var_data) {
  std::stringstream rtn;
  switch (var_data.type) {
    case PP_VARTYPE_STRING:
      rtn << "String with id " << var_data.id <<
             " with value \"" << var_data.string_value << "\"";
      break;
    case PP_VARTYPE_ARRAY:
      rtn << "Array of size " << var_data.array_value.size()
          << " with id " << var_data.id;
      break;
    case PP_VARTYPE_ARRAY_BUFFER:
      rtn << "ArrayBuffer of size " << var_data.buffer_value.length
          << " with id " << var_data.id;
      break;
    case PP_VARTYPE_DICTIONARY:
      rtn << "Dictionary of size " << var_data.dict_value.size() << " with id "
          << var_data.id;
      break;
    default:
      rtn << "resource of type " << var_data.type
          << " with id " << var_data.id;
      break;
  }
  return rtn.str();
}

void FakeVarManager::DestroyVarData_Locked(FakeVarData* var_data) {
  // Release each PP_Var in the array

  switch (var_data->type) {
    case PP_VARTYPE_ARRAY: {
      FakeArrayType& vector = var_data->array_value;
      for (FakeArrayType::iterator it = vector.begin();
           it != vector.end(); ++it) {
        Release_Locked(*it);
      }
      vector.clear();
      break;
    }
    case PP_VARTYPE_ARRAY_BUFFER: {
      free(var_data->buffer_value.ptr);
      var_data->buffer_value.ptr = NULL;
      var_data->buffer_value.length = 0;
      break;
    }
    case PP_VARTYPE_DICTIONARY: {
      FakeDictType& dict = var_data->dict_value;
      for (FakeDictType::iterator it = dict.begin();
           it != dict.end(); it++) {
        Release_Locked(it->second);
      }
      dict.clear();
      break;
    }
    default:
      break;
  }
}

FakeVarData* FakeVarManager::GetVarData(PP_Var var) {
  AUTO_LOCK(lock_);
  return GetVarData_Locked(var);
}

FakeVarData* FakeVarManager::GetVarData_Locked(PP_Var var) {
  switch (var.type) {
    // These types don't have any var data as their data
    // is stored directly in the var's value union.
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
    case PP_VARTYPE_BOOL:
    case PP_VARTYPE_INT32:
    case PP_VARTYPE_DOUBLE:
      return NULL;
    default:
      break;
  }

  VarMap::iterator iter = var_map_.find(var.value.as_id);
  if (iter == var_map_.end())
    return NULL;
  FakeVarData* var_data = &iter->second;
  EXPECT_GT(var_data->ref_count, 0)
      << "Accessing freed " << Describe(*var_data);
  return var_data;
}

void FakeVarManager::Release(PP_Var var) {
  AUTO_LOCK(lock_);
  Release_Locked(var);
}

void FakeVarManager::Release_Locked(PP_Var var) {
  // From ppb_var.h:
  //   Release() removes a reference to given var, deleting it if the internal
  //   reference count becomes 0. If the given var is not a refcounted object,
  //   this function will do nothing so you can always call it no matter what
  //   the type.
  FakeVarData* var_data = GetVarData_Locked(var);
  if (!var_data) {
    if (debug)
      printf("Releasing simple var\n");
    return;
  }

  EXPECT_GT(var_data->ref_count, 0)
      << "Releasing freed " << Describe(*var_data);

  var_data->ref_count--;
  if (debug)
    printf("Released %s [new refcount=%d]\n",
           Describe(*var_data).c_str(),
           var_data->ref_count);

  if (var_data->ref_count == 0)
    DestroyVarData_Locked(var_data);
}
