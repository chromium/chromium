// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_MANAGER_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_MANAGER_H_

#include <map>
#include <ppapi/c/pp_var.h>
#include <string>
#include <vector>

#include "sdk_util/macros.h"
#include "sdk_util/simple_lock.h"

typedef std::vector<PP_Var> FakeArrayType;
typedef std::map<std::string, PP_Var> FakeDictType;

struct FakeVarData {
  uint64_t id;
  uint64_t type;
  int32_t ref_count;
  std::string string_value;
  FakeArrayType array_value;
  FakeDictType dict_value;
  struct {
    void* ptr;
    uint32_t length;
  } buffer_value;
};

class FakeVarManager {
 public:
  FakeVarManager();

  FakeVarManager(const FakeVarManager&) = delete;
  FakeVarManager& operator=(const FakeVarManager&) = delete;

  ~FakeVarManager();

  void AddRef(PP_Var var);
  void Release(PP_Var var);
  FakeVarData* CreateVarData();
  FakeVarData* GetVarData(PP_Var var);
  std::string Describe(const FakeVarData& resource);

  bool debug;
 private:
  void Release_Locked(PP_Var var);
  FakeVarData* GetVarData_Locked(PP_Var var);
  void DestroyVarData_Locked(FakeVarData* var);

  typedef uint64_t Id;
  typedef std::map<Id, FakeVarData> VarMap;

  Id next_id_;
  VarMap var_map_;

  sdk_util::SimpleLock lock_;
};

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_VAR_MANAGER_H_
