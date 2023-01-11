// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/raw_var_data.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/dictionary_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/test_globals.h"
#include "ppapi/shared_impl/test_utils.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ppapi {
namespace proxy {

namespace {

void DefaultHandleWriter(base::Pickle* m, const SerializedHandle& handle) {
  IPC::ParamTraits<SerializedHandle>::Write(m, handle);
}

class RawVarDataTest : public testing::Test {
 public:
  RawVarDataTest() {}
  ~RawVarDataTest() {}

  // testing::Test implementation.
  virtual void SetUp() {
    ProxyLock::Acquire();
  }
  virtual void TearDown() {
    ASSERT_TRUE(PpapiGlobals::Get()->GetVarTracker()->GetLiveVars().empty());
    ProxyLock::Release();
  }

 private:
  base::test::SingleThreadTaskEnvironment
      task_environment_;  // Required to receive callbacks.
  TestGlobals globals_;
};

bool WriteAndRead(const PP_Var& var, PP_Var* result) {
  PP_Instance dummy_instance = 1234;
  std::unique_ptr<RawVarDataGraph> expected_data(
      RawVarDataGraph::Create(var, dummy_instance));
  if (!expected_data)
    return false;
  IPC::Message m;
  expected_data->Write(&m, base::BindRepeating(&DefaultHandleWriter));
  base::PickleIterator iter(m);
  std::unique_ptr<RawVarDataGraph> actual_data(
      RawVarDataGraph::Read(&m, &iter));
  *result = actual_data->CreatePPVar(dummy_instance);
  return true;
}

// Assumes a ref for var.
bool WriteReadAndCompare(const PP_Var& var) {
  ScopedPPVar expected(ScopedPPVar::PassRef(), var);
  PP_Var result;
  bool success = WriteAndRead(expected.get(), &result);
  if (!success)
    return false;
  ScopedPPVar actual(ScopedPPVar::PassRef(), result);
  return TestEqual(expected.get(), actual.get(), true);
}

}  // namespace

TEST_F(RawVarDataTest, SimpleTest) {
  EXPECT_TRUE(WriteReadAndCompare(PP_MakeUndefined()));
  EXPECT_TRUE(WriteReadAndCompare(PP_MakeNull()));
  EXPECT_TRUE(WriteReadAndCompare(PP_MakeInt32(100)));
  EXPECT_TRUE(WriteReadAndCompare(PP_MakeBool(PP_TRUE)));
  EXPECT_TRUE(WriteReadAndCompare(PP_MakeDouble(53.75)));
  PP_Var object;
  object.type = PP_VARTYPE_OBJECT;
  object.value.as_id = 10;
  EXPECT_TRUE(WriteReadAndCompare(object));
}

TEST_F(RawVarDataTest, StringTest) {
  EXPECT_TRUE(WriteReadAndCompare(StringVar::StringToPPVar("")));
  EXPECT_TRUE(WriteReadAndCompare(StringVar::StringToPPVar("hello world!")));
}

TEST_F(RawVarDataTest, ArrayBufferTest) {
  std::string data = "hello world!";
  PP_Var var = PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
      static_cast<uint32_t>(data.size()), data.data());
  EXPECT_TRUE(WriteReadAndCompare(var));
  var = PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferPPVar(
      0, static_cast<void*>(NULL));
  EXPECT_TRUE(WriteReadAndCompare(var));
  // TODO(raymes): add tests for shmem type array buffers.
}

TEST_F(RawVarDataTest, DictionaryArrayTest) {
  // Empty array.
  scoped_refptr<ArrayVar> array(new ArrayVar);
  ScopedPPVar release_array(ScopedPPVar::PassRef(), array->GetPPVar());
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  size_t index = 0;

  // Array with primitives.
  array->Set(static_cast<uint32_t>(index++), PP_MakeUndefined());
  array->Set(static_cast<uint32_t>(index++), PP_MakeNull());
  array->Set(static_cast<uint32_t>(index++), PP_MakeInt32(100));
  array->Set(static_cast<uint32_t>(index++), PP_MakeBool(PP_FALSE));
  array->Set(static_cast<uint32_t>(index++), PP_MakeDouble(0.123));
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  // Array with 2 references to the same string.
  ScopedPPVar release_string(
      ScopedPPVar::PassRef(), StringVar::StringToPPVar("abc"));
  array->Set(static_cast<uint32_t>(index++), release_string.get());
  array->Set(static_cast<uint32_t>(index++), release_string.get());
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  // Array with nested array that references the same string.
  scoped_refptr<ArrayVar> array2(new ArrayVar);
  ScopedPPVar release_array2(ScopedPPVar::PassRef(), array2->GetPPVar());
  array2->Set(0, release_string.get());
  array->Set(static_cast<uint32_t>(index++), release_array2.get());
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  // Empty dictionary.
  scoped_refptr<DictionaryVar> dictionary(new DictionaryVar);
  ScopedPPVar release_dictionary(ScopedPPVar::PassRef(),
                                 dictionary->GetPPVar());
  EXPECT_TRUE(WriteReadAndCompare(dictionary->GetPPVar()));

  // Dictionary with primitives.
  dictionary->SetWithStringKey("1", PP_MakeUndefined());
  dictionary->SetWithStringKey("2", PP_MakeNull());
  dictionary->SetWithStringKey("3", PP_MakeInt32(-100));
  dictionary->SetWithStringKey("4", PP_MakeBool(PP_TRUE));
  dictionary->SetWithStringKey("5", PP_MakeDouble(-103.52));
  EXPECT_TRUE(WriteReadAndCompare(dictionary->GetPPVar()));

  // Dictionary with 2 references to the same string.
  dictionary->SetWithStringKey("6", release_string.get());
  dictionary->SetWithStringKey("7", release_string.get());
  EXPECT_TRUE(WriteReadAndCompare(dictionary->GetPPVar()));

  // Dictionary with nested dictionary that references the same string.
  scoped_refptr<DictionaryVar> dictionary2(new DictionaryVar);
  ScopedPPVar release_dictionary2(ScopedPPVar::PassRef(),
                                  dictionary2->GetPPVar());
  dictionary2->SetWithStringKey("abc", release_string.get());
  dictionary->SetWithStringKey("8", release_dictionary2.get());
  EXPECT_TRUE(WriteReadAndCompare(dictionary->GetPPVar()));

  // Array with dictionary.
  array->Set(static_cast<uint32_t>(index++), release_dictionary.get());
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  // Array with dictionary with array.
  array2->Set(0, PP_MakeInt32(100));
  dictionary->SetWithStringKey("9", release_array2.get());
  EXPECT_TRUE(WriteReadAndCompare(array->GetPPVar()));

  // Array <-> dictionary cycle.
  dictionary->SetWithStringKey("10", release_array.get());
  PP_Var result;
  ASSERT_FALSE(WriteAndRead(release_dictionary.get(), &result));
  // Break the cycle.
  // TODO(raymes): We need some better machinery for releasing vars with
  // cycles. Remove the code below once we have that.
  dictionary->DeleteWithStringKey("10");

  // Array with self references.
  array->Set(static_cast<uint32_t>(index), release_array.get());
  ASSERT_FALSE(WriteAndRead(release_array.get(), &result));
  // Break the self reference.
  array->Set(static_cast<uint32_t>(index), PP_MakeUndefined());
}

TEST_F(RawVarDataTest, ResourceTest) {
  // TODO(mgiuca): This test passes trivially, since GetVarTracker() returns a
  // TestVarTracker which returns a null PP_Var.
  ScopedPPVar resource(
      ScopedPPVar::PassRef(),
      PpapiGlobals::Get()->GetVarTracker()->MakeResourcePPVar(34));
  EXPECT_TRUE(WriteReadAndCompare(resource.get()));

  // TODO(mgiuca): Test a host resource with an IPC::Message. It is currently a
  // checkfail to deserialize such a resource.
}

}  // namespace proxy
}  // namespace ppapi
